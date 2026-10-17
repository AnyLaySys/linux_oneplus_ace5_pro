//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "[sc8517]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/sched/clock.h>
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
#include <soc/oplus/system/oplus_project.h>
#endif
#include <soc/oplus/device_info.h>
#include <oplus_chg_ic.h>
#include <oplus_chg_module.h>
#include <oplus_chg.h>
#include <oplus_mms_wired.h>
#include <oplus_mms.h>
#include <oplus_impedance_check.h>
#include "../oplus_voocphy.h"
#include "../chglib/oplus_chglib.h"
#include "oplus_sc8517.h"
#include "../monitor/oplus_chg_track.h"

struct sc8517_device {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct oplus_voocphy_manager *voocphy;
	struct ufcs_dev *ufcs;

	struct oplus_chg_ic_dev *cp_ic;
	struct oplus_impedance_node *input_imp_node;
	struct oplus_impedance_node *output_imp_node;

	struct mutex i2c_rw_lock;
	struct mutex chip_lock;
	atomic_t suspended;
	atomic_t i2c_err_count;
	struct wakeup_source *chip_ws;

	int ovp_reg;
	int ocp_reg;

	bool ufcs_enable;

	enum oplus_cp_work_mode cp_work_mode;

	bool rested;
	bool error_reported;

	bool use_ufcs_phy;
	bool use_vooc_phy;
	bool vac_support;
};

static enum oplus_cp_work_mode g_cp_support_work_mode[] = {
	CP_WORK_MODE_BYPASS,
};

#define I2C_ERR_MAX	10
static void sc8517_i2c_error(struct oplus_voocphy_manager *voocphy, bool happen)
{
	struct vphy_chip *v_chip = NULL;

	if (!voocphy)
		return;

	if (happen) {
		voocphy->voocphy_iic_err = true;
		voocphy->voocphy_iic_err_num++;
		if (voocphy->voocphy_iic_err_num >= I2C_ERR_MAX) {
			v_chip = oplus_chglib_get_vphy_chip(voocphy->dev);
			if (v_chip)
				oplus_chglib_creat_i2c_err(v_chip->dev); /* CP I2C error */
		}
	} else {
		voocphy->voocphy_iic_err_num = 0;
	}
}

static bool sc8517_check_work_mode_support(enum oplus_cp_work_mode mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g_cp_support_work_mode); i++) {
		if (g_cp_support_work_mode[i] == mode)
			return true;
	}
	return false;
}

static int __sc8517_read_byte(struct i2c_client *client, u8 reg, u8 *data)
{
	s32 ret;
	struct oplus_voocphy_manager *chip = i2c_get_clientdata(client);

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		sc8517_i2c_error(chip, true);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8517_write_byte(struct i2c_client *client, int reg, u8 val)
{
	s32 ret;
	struct oplus_voocphy_manager *chip = i2c_get_clientdata(client);

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		sc8517_i2c_error(chip, true);
		return ret;
	}

	return 0;
}

static int sc8517_read_byte(struct i2c_client *client, u8 reg, u8 *data)
{
	struct sc8517_device *chip;
	struct oplus_voocphy_manager *voocphy = i2c_get_clientdata(client);
	int ret;

	if (voocphy == NULL) {
		return -ENODEV;
	}
	chip = voocphy->priv_data;
	if (chip == NULL) {
		return -ENODEV;
	}

	mutex_lock(&chip->i2c_rw_lock);
	ret = __sc8517_read_byte(client, reg, data);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int sc8517_write_byte(struct i2c_client *client, u8 reg, u8 data)
{
	struct sc8517_device *chip;
	struct oplus_voocphy_manager *voocphy = i2c_get_clientdata(client);
	int ret;

	if (voocphy == NULL) {
		return -ENODEV;
	}
	chip = voocphy->priv_data;
	if (chip == NULL) {
		return -ENODEV;
	}

	mutex_lock(&chip->i2c_rw_lock);
	ret = __sc8517_write_byte(client, reg, data);
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static int sc8517_update_bits(struct i2c_client *client, u8 reg,
                              u8 mask, u8 data)
{
	struct sc8517_device *chip;
	struct oplus_voocphy_manager *voocphy = i2c_get_clientdata(client);
	int ret;
	u8 tmp;

	if (voocphy == NULL) {
		return -ENODEV;
	}
	chip = voocphy->priv_data;
	if (chip == NULL) {
		return -ENODEV;
	}

	mutex_lock(&chip->i2c_rw_lock);
	ret = __sc8517_read_byte(client, reg, &tmp);
	if (ret) {
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8517_write_byte(client, reg, tmp);
out:
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static s32 sc8517_read_word(struct i2c_client *client, u8 reg)
{
	s32 ret;
	struct oplus_voocphy_manager *voocphy = i2c_get_clientdata(client);
	struct sc8517_device *chip;

	if (voocphy == NULL) {
		return -ENODEV;
	}
	chip = voocphy->priv_data;
	if (chip == NULL) {
		return -ENODEV;
	}

	mutex_lock(&chip->i2c_rw_lock);
	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		sc8517_i2c_error(voocphy, true);
		mutex_unlock(&chip->i2c_rw_lock);
		return ret;
	}
	mutex_unlock(&chip->i2c_rw_lock);

	return ret;
}

static s32 sc8517_write_word(struct i2c_client *client, u8 reg, u16 val)
{
	s32 ret;
	struct oplus_voocphy_manager *voocphy = i2c_get_clientdata(client);
	struct sc8517_device *chip;

	if (voocphy == NULL) {
		return -ENODEV;
	}
	chip = voocphy->priv_data;
	if (chip == NULL) {
		return -ENODEV;
	}

	mutex_lock(&chip->i2c_rw_lock);
	ret = i2c_smbus_write_word_data(client, reg, val);
	if (ret < 0) {
		sc8517_i2c_error(voocphy, true);
		mutex_unlock(&chip->i2c_rw_lock);
		return ret;
	}
	mutex_unlock(&chip->i2c_rw_lock);

	return 0;
}

static int sc8517_read_i2c_nonblock(struct i2c_client *client, u8 reg, u8 length, u8 *returnData)
{
	int rc = 0;
	int retry = 3;
	struct oplus_voocphy_manager *voocphy = i2c_get_clientdata(client);

	rc = i2c_smbus_read_i2c_block_data(client, reg, length, returnData);
	if (rc < 0) {
		while (retry > 0) {
			usleep_range(5000, 5000);
			rc = i2c_smbus_read_i2c_block_data(client, reg, length, returnData);
			if (rc < 0)
				retry--;
			else
				break;
		}
	}

	if (rc < 0) {
		sc8517_i2c_error(voocphy, true);
	} else {
		sc8517_i2c_error(voocphy, false);
	}

	return rc;
}


static int sc8517_read_i2c_block(struct i2c_client *client, u8 reg, u8 length, u8 *returnData)
{
	struct sc8517_device *chip;
	struct oplus_voocphy_manager *voocphy = i2c_get_clientdata(client);
	int rc = 0;
	int retry = 3;

	if (voocphy == NULL) {
		return -ENODEV;
	}
	chip = voocphy->priv_data;
	if (chip == NULL) {
		return -ENODEV;
	}

	mutex_lock(&chip->i2c_rw_lock);
	rc = i2c_smbus_read_i2c_block_data(client, reg, length, returnData);

	if (rc < 0) {
		while (retry > 0) {
			usleep_range(5000, 5000);
			rc = i2c_smbus_read_i2c_block_data(client, reg, length, returnData);
			if (rc < 0)
				retry--;
			else
				break;
		}
	}

	if (rc < 0) {
		sc8517_i2c_error(voocphy, true);
	} else {
		sc8517_i2c_error(voocphy, false);
	}
	mutex_unlock(&chip->i2c_rw_lock);

	return rc;
}

static int sc8517_set_predata(struct oplus_voocphy_manager *chip, u16 val)
{
	s32 ret;
	if (!chip) {
		return -1;
	}

	/* predata */
	ret = sc8517_write_word(chip->client, SC8517_REG_2A, val);
	if (ret < 0) {
		return ret;
	}


	return ret;
}

static int sc8517_set_txbuff(struct oplus_voocphy_manager *chip, u16 val)
{
	s32 ret;

	if (!chip) {
		return -1;
	}

	/* txbuff */
	ret = sc8517_write_word(chip->client, SC8517_REG_25, val);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

static int sc8517_get_adapter_info(struct oplus_voocphy_manager *chip)
{
	s32 data;

	if (!chip) {
		return -1;
	}

	data = sc8517_read_word(chip->client, SC8517_REG_27);

	if (data < 0) {
		return data;
	}

	VOOCPHY_DATA16_SPLIT(data, chip->voocphy_rx_buff, chip->vooc_flag);

	return 0;
}

static void sc8517_update_data(struct oplus_voocphy_manager *chip)
{
	/* interrupt_flag */
	/* sc8517_read_byte(chip->client, SC8517_REG_09, &data); */
	chip->interrupt_flag = 0;
	chip->cp_vsys = 0;
	chip->cp_vbat = 0;
	chip->cp_ichg = 0;
	chip->cp_vbus = 0;
}

static int sc8517_cp_vbus(struct oplus_voocphy_manager *chip)
{
	int vbus;
	struct oplus_mms *wired_topic;
	union mms_msg_data data = { 0 };

	wired_topic = oplus_mms_get_by_name("wired");
	if (!wired_topic)
		return 0;

	oplus_mms_get_item_data(wired_topic, WIRED_ITEM_VBUS, &data, true);
	vbus = data.intval;

	return vbus;
}

static int sc8517_get_cp_ichg(struct oplus_voocphy_manager *chip)
{
	return 0;
}

int sc8517_get_cp_vbat(struct oplus_voocphy_manager *chip)
{
	return 0;
}

static int sc8517_reg_reset(struct oplus_voocphy_manager *chip, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SC8517_RESET_REG;
	else
		val = SC8517_NO_REG_RESET;

	val <<= SC8517_REG_RESET_SHIFT;

	ret = sc8517_update_bits(chip->client, SC8517_REG_06,
	                         SC8517_REG_RESET_MASK, val);


	return ret;
}

static int sc8517_get_chg_enable(struct oplus_voocphy_manager *chip, u8 *data)
{
	int ret = 0;

	if (!chip) {
		return -1;
	}

	ret = sc8517_read_byte(chip->client, SC8517_REG_02, data);
	if (ret < 0) {
		return ret;
	}

	*data = *data & SC8517_CHG_EN_MASK;

	return ret;
}

static int sc8517_set_chg_enable(struct oplus_voocphy_manager *chip, bool enable)
{
	int ret = 0;

	if (!chip) {
		return -1;
	}

	/* vac range disable */
	sc8517_write_byte(chip->client, SC8517_REG_02, 0x7a);

	if (enable)
		ret = sc8517_write_byte(chip->client, SC8517_REG_02, ENABLE_MOS); /* enable mos */
	else
		ret = sc8517_write_byte(chip->client, SC8517_REG_02, DISENABLE_MOS); /* disable mos */

	if (ret < 0) {
		return ret;
	}


	return ret;
}


static int sc8517_get_adc_enable(struct oplus_voocphy_manager *chip, u8 *data)
{
	return 0;
}

static int sc8517_set_adc_enable(struct oplus_voocphy_manager *chip, bool enable)
{
	return 0;
}

static u8 sc8517_get_vbus_status(struct oplus_voocphy_manager *chip)
{
	int ret = 0;
	u8 value = 0;

	if (!chip) {
		return -1;
	}

	ret = sc8517_read_byte(chip->client, SC8517_REG_0B, &value);
	if (ret < 0) {
		return ret;
	}

	value = (value & VBUS_INRANGE_STATUS_MASK) >> VBUS_INRANGE_STATUS_SHIFT;

	return value;
}

static int sc8517_get_voocphy_enable(struct oplus_voocphy_manager *chip, u8 *data)
{
	int ret = 0;

	if (!chip) {
		return -1;
	}

	ret = sc8517_read_byte(chip->client, SC8517_REG_2B, data);

	return ret;
}

static void sc8517_dump_reg_in_err_issue(struct oplus_voocphy_manager *chip)
{
	if (!chip) {
		return;
	}


	return;
}

static u8 sc8517_get_int_value(struct oplus_voocphy_manager *chip)
{
	int ret = 0;
	u8 int_column[6];

	if (!chip) {
		return -1;
	}

	ret = sc8517_read_i2c_block(chip->client, SC8517_REG_09, 6, int_column);
	if (ret < 0) {
		sc8517_i2c_error(chip, true);
		memset(chip->int_column, 0, sizeof(chip->int_column));
		/* set int_column[1]=1, otherwise the dcdc chg can't be enabled*/
		chip->int_column[1] = BIT(0);
		return chip->int_column[1];
	}

	memcpy(chip->int_column, int_column, sizeof(chip->int_column));

	return int_column[1];
}

static u8 sc8517_get_chg_auto_mode(struct oplus_voocphy_manager *chip)
{
	int ret = 0;
	u8 value = 0;

	if (!chip) {
		return -1;
	}
	ret = sc8517_read_byte(chip->client, SC8517_REG_15, &value);
	value = value & SC8517_CHG_MODE_MASK;

	return value;
}

static int sc8517_set_chg_auto_mode(struct oplus_voocphy_manager *chip, bool enable)
{
	int ret = 0;

	if (!chip) {
		return -1;
	}


	if (enable && (sc8517_get_chg_auto_mode(chip) == SC8517_CHG_FIX_MODE))
		ret = sc8517_update_bits(chip->client, SC8517_REG_15,
					 SC8517_CHG_MODE_MASK,
					 SC8517_CHG_AUTO_MODE);
	else if (!enable && (sc8517_get_chg_auto_mode(chip) == SC8517_CHG_AUTO_MODE))
		ret = sc8517_update_bits(chip->client, SC8517_REG_15,
					 SC8517_CHG_MODE_MASK,
					 SC8517_CHG_FIX_MODE);

	return ret;
}

static int sc8517_set_vac2v2x_uvp(struct oplus_voocphy_manager *chip, bool disable)
{
	int ret = 0;
	u8 data = 0;

	if (!chip) {
		return -1;
	}


	if (disable)
		ret |= sc8517_update_bits(chip->client, SC8517_REG_7D,
				 SC8517_CHG_DIS_VAC2V2X_UV_MASK,
				 SC8517_CHG_DIS_VAC2V2X_UV_DIS << SC8517_CHG_DIS_VAC2V2X_UV_SHIFT);
	else
		ret |= sc8517_update_bits(chip->client, SC8517_REG_7D,
				 SC8517_CHG_DIS_VAC2V2X_UV_MASK,
				 SC8517_CHG_DIS_VAC2V2X_UV_EN << SC8517_CHG_DIS_VAC2V2X_UV_SHIFT);

	if (ret < 0) {
	} else {
		sc8517_read_byte(chip->client, SC8517_REG_7D, &data);
	}

	return ret;
}

static void sc8517_set_pd_svooc_config(struct oplus_voocphy_manager *chip, bool enable)
{
	if (!chip) {
		return;
	}

	sc8517_write_byte(chip->client, SC8517_REG_04, 0x36); /* WD:1000ms */
	sc8517_write_byte(chip->client, SC8517_REG_2C, 0xd1); /* Loose_det=1 */

}

static bool sc8517_get_pd_svooc_config(struct oplus_voocphy_manager *chip)
{
	if (!chip) {
		return false;
	}

	return true;
}

void sc8517_send_handshake(struct oplus_voocphy_manager *chip)
{
	/* enable voocphy and handshake */
	sc8517_write_byte(chip->client, SC8517_REG_24, 0x81);
}


static int sc8517_reset_voocphy(struct oplus_voocphy_manager *chip)
{
	/* turn off mos */
	sc8517_write_byte(chip->client, SC8517_REG_02, 0x78);
	/* clear tx data */
	sc8517_write_byte(chip->client, SC8517_REG_25, 0x00);
	sc8517_write_byte(chip->client, SC8517_REG_26, 0x00);
	/* disable vooc phy irq */
	sc8517_write_byte(chip->client, SC8517_REG_29, 0x7F); /* mask all flag */
	sc8517_write_byte(chip->client, SC8517_REG_10, 0x79); /* disable irq */
	/* set D+ HiZ */
	sc8517_write_byte(chip->client, SC8517_REG_20, 0xc0);

	/* select big bang mode */
	/* disable vooc */
	sc8517_write_byte(chip->client, SC8517_REG_24, 0x00);
	sc8517_write_byte(chip->client, SC8517_REG_04, 0x06); /* disable wdt */

	/* set predata */
	/* sc8517_write_word(chip->client, SC8517_REG_2A, 0x0); */
	sc8517_write_byte(chip->client, SC8517_REG_24, 0x02); /* reset voocphy */
	msleep(1);
	/* sc8517_set_predata(0x0); */

	return VOOCPHY_SUCCESS;
}


static int sc8517_reactive_voocphy(struct oplus_voocphy_manager *chip)
{
	/* set predata 0 */
	sc8517_write_word(chip->client, SC8517_REG_2A, 0x0);

	/* dpdm */
	sc8517_write_byte(chip->client, SC8517_REG_20, 0x21);
	sc8517_write_byte(chip->client, SC8517_REG_21, 0x80);
	sc8517_write_byte(chip->client, SC8517_REG_2C, 0xD1);

	/* clear tx data */
	sc8517_write_byte(chip->client, SC8517_REG_25, 0x00);
	sc8517_write_byte(chip->client, SC8517_REG_26, 0x00);

	/* vooc */
	sc8517_write_byte(chip->client, SC8517_REG_29, 0x05); /* diable vooc int flag mask */
	sc8517_send_handshake(chip);


	return VOOCPHY_SUCCESS;
}

static int sc8517_init_device(struct oplus_voocphy_manager *chip)
{
	sc8517_reg_reset(chip, true);
	msleep(10);
	sc8517_write_byte(chip->client, SC8517_REG_24, 0x00); /* VOOC_CTRL:disable,no handshake */

	return 0;
}

static int sc8517_init_vooc(struct oplus_voocphy_manager *chip)
{

	sc8517_write_byte(chip->client, SC8517_REG_24, 0x02); /* reset voocphy */
	sc8517_write_word(chip->client, SC8517_REG_2A, 0x00); /* reset predata 00 */
	msleep(1);
	sc8517_write_byte(chip->client, SC8517_REG_20, 0x21); /* VOOC_CTRL:disable,no handshake */
	sc8517_write_byte(chip->client, SC8517_REG_21, 0x80); /* VOOC_CTRL:disable,no handshake */
	sc8517_write_byte(chip->client, SC8517_REG_2C, 0xD1); /* VOOC_CTRL:disable,no handshake */
	sc8517_write_byte(chip->client, SC8517_REG_29, 0x25); /* mask sedseq flag,rx start,tx done */
	/* sc8517_vac_inrange_enable(chip, SC8517_VAC_INRANGE_DISABLE); */

	return 0;
}

static void sc8517_hardware_init(struct oplus_voocphy_manager *chip)
{
	sc8517_reg_reset(chip, true);
	msleep(10);
	sc8517_write_byte(chip->client, SC8517_REG_01, 0x5e); /* disable v1x_scp */
	sc8517_write_byte(chip->client, SC8517_REG_06, 0x85); /* enable  audio mode */
	sc8517_write_byte(chip->client, SC8517_REG_08, 0xA6); /* REF_SKIP_R 40mv */
	sc8517_write_byte(chip->client, SC8517_REG_29, 0x05); /* Masked Pulse_filtered, RX_Start,Tx_Done,soft intflag */
	sc8517_write_byte(chip->client, SC8517_REG_10, 0x79); /* Masked Pulse_filtered, RX_Start,Tx_Done */
	sc8517_write_byte(chip->client, SC8517_REG_03, 0xFF); /* set rvs and fwd ocp */
	sc8517_write_byte(chip->client, SC8517_REG_12, 0x10); /* set OCP trigger time to 10us */
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	if (get_eng_version() == HIGH_TEMP_AGING)
		sc8517_write_byte(chip->client, SC8517_REG_0F, 0x06); /* mask 100/120 TDIE alarm */
#endif
	if (chip->v2x_volt_full_open_low) {
		sc8517_update_bits(chip->client, SC8517_REG_00, 0xc, 0); /* if device power off voltage is lower than 3V, need change 8517 full open thr */
	}
}

static int sc8517_dump_registers(struct oplus_voocphy_manager *chip)
{
	int ret = 0;
	u8 int_column[7];

	if (!chip) {
		return -1;
	}

	ret = sc8517_read_i2c_block(chip->client, SC8517_REG_00, 7, int_column);
	if (ret < 0) {
		return ret;
	}

	ret = sc8517_read_i2c_block(chip->client, SC8517_REG_07, 7, int_column);
	if (ret < 0) {
		return ret;
	}

	ret = sc8517_read_i2c_block(chip->client, SC8517_REG_0E, 7, int_column);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

static int sc8517_svooc_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8517_write_byte(chip->client, SC8517_REG_04, 0x36); /* WD:1000ms */
	sc8517_write_byte(chip->client, SC8517_REG_2C, 0xd1); /* Loose_det=1 */

	return 0;
}

static int sc8517_vooc_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8517_write_byte(chip->client, SC8517_REG_04, 0x46); /* WD:5000ms */
	sc8517_write_byte(chip->client, SC8517_REG_2C, 0xd1); /* Loose_det=1 */

	return 0;
}

static int sc8517_5v2a_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8517_write_byte(chip->client, SC8517_REG_02, 0x78); /* disable acdrv,close mos */
	sc8517_write_byte(chip->client, SC8517_REG_04, 0x06); /* dsiable wdt */
	sc8517_write_byte(chip->client, SC8517_REG_24, 0x00); /* close voocphy */

	return 0;
}

static int sc8517_pdqc_hw_setting(struct oplus_voocphy_manager *chip)
{
	sc8517_write_byte(chip->client, SC8517_REG_02, 0x78); /* disable acdrv,close mos */
	sc8517_write_byte(chip->client, SC8517_REG_04, 0x06); /* dsiable wdt */
	sc8517_write_byte(chip->client, SC8517_REG_24, 0x00); /* close voocphy */

	return 0;
}

static int sc8517_hw_setting(struct oplus_voocphy_manager *chip, int reason)
{
	if (!chip) {
		return -1;
	}
	switch (reason) {
	case SETTING_REASON_PROBE:
	case SETTING_REASON_RESET:
		sc8517_init_device(chip);
		break;
	case SETTING_REASON_SVOOC:
		sc8517_svooc_hw_setting(chip);
		break;
	case SETTING_REASON_VOOC:
		sc8517_vooc_hw_setting(chip);
		break;
	case SETTING_REASON_5V2A:
		sc8517_5v2a_hw_setting(chip);
		break;
	case SETTING_REASON_PDQC:
		sc8517_pdqc_hw_setting(chip);
		break;
	default:
		;
		break;
	}
	return 0;
}

static bool sc8517_check_cp_int_happened(struct oplus_voocphy_manager *chip,
					 bool *dump_reg, bool *send_info)
{
	int i = 0;

	if (((bidirect_int_flag[0].mask & chip->int_column_pre[1]) == 0) && bidirect_int_flag[i].mark_except) {
		*dump_reg = true;
		*send_info = true;
		memcpy(&chip->reg_dump[9], chip->int_column_pre, sizeof(chip->int_column_pre));
		return true;
	}

	for (i = 1; i < 7; i++) {
		if ((bidirect_int_flag[i].mask & chip->int_column_pre[3]) && bidirect_int_flag[i].mark_except) {
			*dump_reg = true;
			*send_info = true;
			memcpy(&chip->reg_dump[9], chip->int_column_pre, sizeof(chip->int_column_pre));
			return true;
		}
	}

	for (i = 7; i < BIDIRECT_IRQ_EVNET_NUM; i++) {
		if ((bidirect_int_flag[i].mask & chip->int_column_pre[5]) && bidirect_int_flag[i].mark_except) {
			*dump_reg = true;
			*send_info = true;
			memcpy(&chip->reg_dump[9], chip->int_column_pre, sizeof(chip->int_column_pre));
			return true;
		}
	}

	return false;
}

#define TRACK_LOCAL_T_NS_TO_S_THD		1000000000
#define TRACK_UPLOAD_COUNT_MAX			10
#define TRACK_DEVICE_ABNORMAL_UPLOAD_PERIOD	(24 * 3600)
#define REASON_LENGTH_MAX			1024
#define ERR_LENGTH_MAX				64
#define DUMP_LENGTH_MAX				512

static int sc8517_track_get_local_time_s(void)
{
	int local_time_s;

	local_time_s = local_clock() / TRACK_LOCAL_T_NS_TO_S_THD;
	return local_time_s;
}

static int sc8517_get_int_reg_info(struct oplus_voocphy_manager *chip,
				   char *dump_info, int len)
{
	int index = 0;

	if (!chip || !dump_info)
		return 0;

	index += snprintf(&(dump_info[index]), len - index,
			  "REG_09~REG_0E:[0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x]",
			  chip->int_column_pre[0], chip->int_column_pre[1], chip->int_column_pre[2],
			  chip->int_column_pre[3], chip->int_column_pre[4], chip->int_column_pre[5]);

	return index;
}

static int sc8517_get_cp_error_type(struct oplus_voocphy_manager *chip,
				    int *err_type)
{
	int i = 0;

	if (NULL == chip || NULL == err_type) {
		return -EINVAL;
	}

	if (((bidirect_int_flag[0].mask & chip->int_column_pre[1]) == 0) &&
	    bidirect_int_flag[i].mark_except) {
		*err_type = i + 1;
		return 0;
	}

	for (i = 1; i < 7; i++) {
		if ((bidirect_int_flag[i].mask & chip->int_column_pre[3]) &&
		    bidirect_int_flag[i].mark_except) {
			*err_type = i + 1;
			return 0;
		}
	}

	for (i = 7; i < BIDIRECT_IRQ_EVNET_NUM; i++) {
		if ((bidirect_int_flag[i].mask & chip->int_column_pre[5]) &&
		    bidirect_int_flag[i].mark_except) {
			*err_type = i + 1;
			return 0;
		}
	}

	return 1; /* not found error type */
}

static int sc8517_track_upload_cp_err_info(struct oplus_voocphy_manager *chip,
					   int err_type)
{
	int index = 0;
	int curr_time;
	static int upload_count = 0;
	static int pre_upload_time = 0;
	char temp_str[REASON_LENGTH_MAX] = {0};
	struct oplus_mms *err_topic;
	struct mms_msg *msg = NULL;
	int rc = 0;
	char err_reason[ERR_LENGTH_MAX] = {0};
	char dump_info[DUMP_LENGTH_MAX] = {0};

	if (NULL == chip) {
		return -EINVAL;
	}

	if (err_type <= TRACK_BIDIRECT_CP_ERR_DEFAULT) {
		return -EINVAL;
	}

	err_topic = oplus_mms_get_by_name("error");
	if (!err_topic) {
		return -EINVAL;
	}

	curr_time = sc8517_track_get_local_time_s();
	if (curr_time - pre_upload_time > TRACK_DEVICE_ABNORMAL_UPLOAD_PERIOD)
		upload_count = 0;

	if (upload_count > TRACK_UPLOAD_COUNT_MAX) {
		return 0;
	}

	upload_count++;
	pre_upload_time = sc8517_track_get_local_time_s();

	index += scnprintf(&(temp_str[index]), REASON_LENGTH_MAX - index, "$$device_id@@%s", "sc8517");
	index += scnprintf(&(temp_str[index]), REASON_LENGTH_MAX - index, "$$err_scene@@%s",
			  OPLUS_CHG_TRACK_SCENE_BIDIRECT_CP_ERR);

	oplus_chg_track_get_bidirect_cp_err_reason(err_type, err_reason, sizeof(err_reason));
	index += scnprintf(&(temp_str[index]), REASON_LENGTH_MAX - index,
			  "$$err_reason@@%s", err_reason);

	sc8517_get_int_reg_info(chip, dump_info, sizeof(dump_info));
	index += scnprintf(&(temp_str[index]), REASON_LENGTH_MAX - index,
			  "$$reg_info@@%s", dump_info);

	msg = oplus_mms_alloc_str_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
				      ERR_ITEM_BIDIRECT_CP_INFO, temp_str);
	if (msg == NULL) {
		return -EINVAL;
	}
	rc = oplus_mms_publish_msg_sync(err_topic, msg);
	if (rc < 0) {
		kfree(msg);
	}

	return 0;
}

static ssize_t sc8517_show_registers(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
	struct oplus_voocphy_manager *chip = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8517");
	for (addr = 0x0; addr <= 0x3C; addr++) {
		if ((addr < 0x24) || (addr > 0x2B && addr < 0x33)
		    || addr == 0x36 || addr == 0x3C) {
			ret = sc8517_read_byte(chip->client, addr, &val);
			if (ret == 0) {
				len = snprintf(tmpbuf, PAGE_SIZE - idx,
				               "Reg[%.2X] = 0x%.2x\n", addr, val);
				memcpy(&buf[idx], tmpbuf, len);
				idx += len;
			}
		}
	}

	return idx;
}

static ssize_t sc8517_store_register(struct device *dev,
                                     struct device_attribute *attr, const char *buf, size_t count)
{
	struct oplus_voocphy_manager *chip = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x3C)
		sc8517_write_byte(chip->client, (unsigned char)reg, (unsigned char)val);

	return count;
}


static DEVICE_ATTR(registers, 0660, sc8517_show_registers, sc8517_store_register);

static void sc8517_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}


static struct of_device_id sc8517_charger_match_table[] = {
	{
		.compatible = "sc,sc8517-master",
	},
	{},
};

static void register_voocphy_devinfo(void)
{
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
	int ret = 0;
	char *version;
	char *manufacture;

	version = "sc8517";
	manufacture = "MP";

	ret = register_device_proc("voocphy", version, manufacture);
#endif
}

static struct oplus_voocphy_operations oplus_sc8517_ops = {
	.hardware_init		= sc8517_hardware_init,
	.hw_setting		= sc8517_hw_setting,
	.init_vooc		= sc8517_init_vooc,
	.set_predata		= sc8517_set_predata,
	.set_txbuff		= sc8517_set_txbuff,
	.get_adapter_info	= sc8517_get_adapter_info,
	.update_data		= sc8517_update_data,
	.get_chg_enable		= sc8517_get_chg_enable,
	.set_chg_enable		= sc8517_set_chg_enable,
	.reset_voocphy		= sc8517_reset_voocphy,
	.reactive_voocphy	= sc8517_reactive_voocphy,
	.send_handshake		= sc8517_send_handshake,
	.get_cp_vbat		= sc8517_get_cp_vbat,
	.get_int_value		= sc8517_get_int_value,
	.get_adc_enable		= sc8517_get_adc_enable,
	.set_adc_enable		= sc8517_set_adc_enable,
	.get_ichg		= sc8517_get_cp_ichg,
	.set_pd_svooc_config	= sc8517_set_pd_svooc_config,
	.get_pd_svooc_config	= sc8517_get_pd_svooc_config,
	.get_vbus_status	= sc8517_get_vbus_status,
	.set_chg_auto_mode	= sc8517_set_chg_auto_mode,
	.get_voocphy_enable	= sc8517_get_voocphy_enable,
	.dump_voocphy_reg	= sc8517_dump_reg_in_err_issue,
	.check_cp_int_happened	= sc8517_check_cp_int_happened,
	.upload_cp_error	= sc8517_track_upload_cp_err_info,
	.get_cp_error_type	= sc8517_get_cp_error_type,
	.cp_set_vac2v2x_uvp	= sc8517_set_vac2v2x_uvp,
};

static irqreturn_t sc8517_interrupt_handler(int irq, void *dev_id)
{
	struct oplus_voocphy_manager *voocphy = dev_id;

	return oplus_voocphy_interrupt_handler(voocphy);
}

static int sc8517_parse_dt(struct oplus_voocphy_manager *chip)
{
	struct device_node *node = chip->dev->of_node;

	chip->v2x_volt_full_open_low = of_property_read_bool(node, "oplus,v2x_volt_full_open_low");

	return 0;
}

static int sc8517_irq_gpio_init(struct oplus_voocphy_manager *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;

	chip->irq_gpio = of_get_named_gpio(node, "oplus,irq_gpio", 0);
	if (!gpio_is_valid(chip->irq_gpio)) {
		chip->irq_gpio = of_get_named_gpio(node, "oplus_spec,irq_gpio", 0);
		if (!gpio_is_valid(chip->irq_gpio)) {
			return chip->irq_gpio;
		}
	}
	rc = gpio_request(chip->irq_gpio, "irq_gpio");
	if (rc) {
		return rc;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	chip->pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
		return -EINVAL;
	}

	chip->charging_inter_active =
	    pinctrl_lookup_state(chip->pinctrl, "charging_inter_active");
	if (IS_ERR_OR_NULL(chip->charging_inter_active)) {
		return -EINVAL;
	}

	chip->charging_inter_sleep =
	    pinctrl_lookup_state(chip->pinctrl, "charging_inter_sleep");
	if (IS_ERR_OR_NULL(chip->charging_inter_sleep)) {
		return -EINVAL;
	}

	gpio_direction_input(chip->irq_gpio);
	pinctrl_select_state(chip->pinctrl, chip->charging_inter_active); /* no_PULL */
	rc = gpio_get_value(chip->irq_gpio);

	return 0;
}

static int sc8517_irq_register(struct sc8517_device *chip)
{
	struct oplus_voocphy_manager *voocphy = chip->voocphy;
	struct cpumask current_mask;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	cpumask_var_t cpu_highcap_mask;
#endif
	int ret;

	ret = sc8517_irq_gpio_init(voocphy);
	if (ret < 0) {
		return ret;
	}

	if (voocphy->irq) {
		ret = request_threaded_irq(voocphy->irq, NULL,
					   sc8517_interrupt_handler,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					   "voocphy_irq", voocphy);
		if (ret < 0) {
			return ret;
		}
		enable_irq_wake(voocphy->irq);
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	update_highcap_mask(cpu_highcap_mask);
	cpumask_and(&current_mask, cpu_online_mask, cpu_highcap_mask);
#else
	cpumask_setall(&current_mask);
	cpumask_and(&current_mask, cpu_online_mask, &current_mask);
#endif
	ret = irq_set_affinity(voocphy->irq, &current_mask);

	return 0;
}
static int sc8517_cp_init(struct oplus_chg_ic_dev *ic_dev)
{
	if (ic_dev == NULL) {
		return -ENODEV;
	}
	ic_dev->online = true;
	oplus_chg_ic_virq_trigger(ic_dev, OPLUS_IC_VIRQ_ONLINE);

	return 0;
}

static int sc8517_cp_exit(struct oplus_chg_ic_dev *ic_dev)
{
	if (ic_dev == NULL) {
		return -ENODEV;
	}
	ic_dev->online = false;
	oplus_chg_ic_virq_trigger(ic_dev, OPLUS_IC_VIRQ_OFFLINE);

	return 0;
}

static int sc8517_cp_reg_dump(struct oplus_chg_ic_dev *ic_dev)
{
	struct sc8517_device *chip;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);

	sc8517_dump_reg_in_err_issue(chip->voocphy);
	return 0;
}

static int sc8517_cp_smt_test(struct oplus_chg_ic_dev *ic_dev, char buf[], int len)
{
	return 0;
}

static int sc8517_cp_enable(struct oplus_chg_ic_dev *ic_dev, bool en)
{
	return 0;
}

static int sc8517_cp_hw_init(struct oplus_chg_ic_dev *ic_dev)
{
	struct sc8517_device *chip;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);

	if (chip->rested)
		return 0;

	sc8517_hardware_init(chip->voocphy);
	return 0;
}

static int sc8517_cp_set_work_mode(struct oplus_chg_ic_dev *ic_dev, enum oplus_cp_work_mode mode)
{
	struct sc8517_device *chip;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);
	rc = sc8517_svooc_hw_setting(chip->voocphy);

	return rc;
}

static int sc8517_cp_get_work_mode(struct oplus_chg_ic_dev *ic_dev, enum oplus_cp_work_mode *mode)
{
	if (ic_dev == NULL) {
		return -ENODEV;
	}
	*mode = CP_WORK_MODE_BYPASS;

	return 0;
}

static int sc8517_cp_check_work_mode_support(struct oplus_chg_ic_dev *ic_dev, enum oplus_cp_work_mode mode)
{
	if (ic_dev == NULL) {
		return -ENODEV;
	}
	return sc8517_check_work_mode_support(mode);
}

static int sc8517_cp_set_iin(struct oplus_chg_ic_dev *ic_dev, int iin)
{
	return 0;
}

static int sc8517_cp_get_vin(struct oplus_chg_ic_dev *ic_dev, int *vin)
{
	struct sc8517_device *chip;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);

	rc = sc8517_cp_vbus(chip->voocphy);
	if (rc < 0) {
		return rc;
	}
	*vin = rc;

	return 0;
}

static int sc8517_cp_get_vout(struct oplus_chg_ic_dev *ic_dev, int *vout)
{
	struct sc8517_device *chip;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);

	rc = sc8517_get_cp_vbat(chip->voocphy);
	if (rc < 0) {
		return rc;
	}
	*vout = rc;

	return 0;
}

static int sc8517_cp_get_iout(struct oplus_chg_ic_dev *ic_dev, int *iout)
{
	struct sc8517_device *chip;
	int iin;
	bool working;
	enum oplus_cp_work_mode work_mode;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);

	/*
	 * There is an exception in the iout adc of sc8537a, which is obtained
	 * indirectly through iin
	 */
	rc = oplus_chg_ic_func(ic_dev, OPLUS_IC_FUNC_CP_GET_WORK_STATUS, &working);
	if (rc < 0)
		return rc;
	if (!working) {
		*iout = 0;
		return 0;
	}
	rc = oplus_chg_ic_func(ic_dev, OPLUS_IC_FUNC_CP_GET_IIN, &iin);
	if (rc < 0)
		return rc;
	rc = oplus_chg_ic_func(ic_dev, OPLUS_IC_FUNC_CP_GET_WORK_MODE, &work_mode);
	if (rc < 0)
		return rc;
	switch (work_mode) {
	case CP_WORK_MODE_BYPASS:
		*iout = iin;
		break;
	case CP_WORK_MODE_2_TO_1:
		*iout = iin * 2;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sc8517_cp_get_vac(struct oplus_chg_ic_dev *ic_dev, int *vac)
{
	struct sc8517_device *chip;
	/* u8 data_block[2] = { 0 }; */
	int rc = 0;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);
	if (!chip->vac_support)
		return -ENOTSUPP;

	/* rc = i2c_smbus_read_i2c_block_data(chip->client, SC8547_REG_17, 2, data_block); */
	if (rc < 0) {
		/* sc8547_i2c_error(chip->voocphy, true); */
		return rc;
	} else {
		/* sc8547_i2c_error(chip->voocphy, false); */
	}

	/* *vac = (((data_block[0] & SC8547_VAC_POL_H_MASK) << 8) | data_block[1]) * SC8547_VAC_ADC_LSB; */

	return 0;
}

static int sc8517_cp_set_work_start(struct oplus_chg_ic_dev *ic_dev, bool start)
{
	struct sc8517_device *chip;
	int rc = 0;
	u8 data = 0;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);
	if (NULL == chip || NULL == chip->voocphy)
		return -ENODEV;

	sc8517_read_byte(chip->voocphy->client, SC8517_REG_02, &data);

	if (start && data == DISENABLE_MOS) {
		rc = sc8517_set_chg_enable(chip->voocphy, start);
		sc8517_write_byte(chip->client, SC8517_REG_04, 0x36); /* WD:1000ms */
	} else if (!start) {
		rc = sc8517_set_chg_enable(chip->voocphy, start);
		sc8517_write_byte(chip->client, SC8517_REG_04, 0x06); /* dsiable wdt */
	}

	if (rc < 0)
		return rc;

	oplus_imp_node_set_active(chip->input_imp_node, start);
	/*oplus_imp_node_set_active(chip->output_imp_node, start); */

	return 0;
}

static int sc8517_cp_get_work_status(struct oplus_chg_ic_dev *ic_dev, bool *start)
{
	struct sc8517_device *chip;
	u8 data;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	chip = oplus_chg_ic_get_priv_data(ic_dev);

	rc = sc8517_get_chg_enable(chip->voocphy, &data);
	if (rc < 0) {
		return rc;
	}

	*start = data & 1;

	return 0;
}

static void *sc8517_cp_get_func(struct oplus_chg_ic_dev *ic_dev, enum oplus_chg_ic_func func_id)
{
	void *func = NULL;

	if (!ic_dev->online && (func_id != OPLUS_IC_FUNC_INIT) &&
	    (func_id != OPLUS_IC_FUNC_EXIT)) {
		return NULL;
	}

	switch (func_id) {
	case OPLUS_IC_FUNC_INIT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_INIT, sc8517_cp_init);
		break;
	case OPLUS_IC_FUNC_EXIT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_EXIT, sc8517_cp_exit);
		break;
	case OPLUS_IC_FUNC_REG_DUMP:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_REG_DUMP, sc8517_cp_reg_dump);
		break;
	case OPLUS_IC_FUNC_SMT_TEST:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_SMT_TEST, sc8517_cp_smt_test);
		break;
	case OPLUS_IC_FUNC_CP_ENABLE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_ENABLE, sc8517_cp_enable);
		break;
	case OPLUS_IC_FUNC_CP_HW_INTI:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_HW_INTI, sc8517_cp_hw_init);
		break;
	case OPLUS_IC_FUNC_CP_SET_WORK_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_SET_WORK_MODE, sc8517_cp_set_work_mode);
		break;
	case OPLUS_IC_FUNC_CP_GET_WORK_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_GET_WORK_MODE, sc8517_cp_get_work_mode);
		break;
	case OPLUS_IC_FUNC_CP_CHECK_WORK_MODE_SUPPORT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_CHECK_WORK_MODE_SUPPORT,
			sc8517_cp_check_work_mode_support);
		break;
	case OPLUS_IC_FUNC_CP_SET_IIN:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_SET_IIN, sc8517_cp_set_iin);
		break;
	case OPLUS_IC_FUNC_CP_GET_VIN:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_GET_VIN, sc8517_cp_get_vin);
		break;
	case OPLUS_IC_FUNC_CP_GET_VOUT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_GET_VOUT, sc8517_cp_get_vout);
		break;
	case OPLUS_IC_FUNC_CP_GET_IOUT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_GET_IOUT, sc8517_cp_get_iout);
		break;
	case OPLUS_IC_FUNC_CP_GET_VAC:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_GET_VAC, sc8517_cp_get_vac);
		break;
	case OPLUS_IC_FUNC_CP_SET_WORK_START:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_SET_WORK_START, sc8517_cp_set_work_start);
		break;
	case OPLUS_IC_FUNC_CP_GET_WORK_STATUS:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_CP_GET_WORK_STATUS, sc8517_cp_get_work_status);
		break;
	default:
		;
		func = NULL;
		break;
	}

	return func;
}

struct oplus_chg_ic_virq sc8517_cp_virq_table[] = {
	{ .virq_id = OPLUS_IC_VIRQ_ERR },
	{ .virq_id = OPLUS_IC_VIRQ_ONLINE },
	{ .virq_id = OPLUS_IC_VIRQ_OFFLINE },
};

static int sc8517_ic_register(struct sc8517_device *chip)
{
	enum oplus_chg_ic_type ic_type;
	int ic_index;
	struct device_node *child;
	struct oplus_chg_ic_dev *ic_dev = NULL;
	struct oplus_chg_ic_cfg ic_cfg;
	int rc;

	for_each_child_of_node(chip->dev->of_node, child) {
		rc = of_property_read_u32(child, "oplus,ic_type", &ic_type);
		if (rc < 0)
			continue;
		rc = of_property_read_u32(child, "oplus,ic_index", &ic_index);
		if (rc < 0)
			continue;
		ic_cfg.name = child->name;
		ic_cfg.index = ic_index;
		ic_cfg.type = ic_type;
		ic_cfg.priv_data = chip;
		ic_cfg.of_node = child;
		switch (ic_type) {
		case OPLUS_CHG_IC_CP:
			/* (void)sc8517_init_imp_node(chip, child); */
			snprintf(ic_cfg.manu_name, OPLUS_CHG_IC_MANU_NAME_MAX - 1, "cp-sc8517:%d", ic_index);
			snprintf(ic_cfg.fw_id, OPLUS_CHG_IC_FW_ID_MAX - 1, "0x00");
			ic_cfg.get_func = sc8517_cp_get_func;
			ic_cfg.virq_data = sc8517_cp_virq_table;
			ic_cfg.virq_num = ARRAY_SIZE(sc8517_cp_virq_table);
			break;
		default:
			;
			continue;
		}
		ic_dev = devm_oplus_chg_ic_register(chip->dev, &ic_cfg);
		if (!ic_dev) {
			rc = -ENODEV;
			continue;
		}

		switch (ic_dev->type) {
		case OPLUS_CHG_IC_CP:
			chip->cp_work_mode = CP_WORK_MODE_UNKNOWN;
			chip->cp_ic = ic_dev;
			break;
		default:
			;
			continue;
		}

		of_platform_populate(child, NULL, NULL, chip->dev);
	}

	return 0;
}

static bool sc8517_check_device_is_exist(struct i2c_client *client)
{
	u8 reg[2] = { 0 };
	int max_count = 5;
	int ret = -1;

	if (NULL == client) {
		return false;
	}

        /* check if the ic is ok by read register */
	while (max_count--) {
		ret = sc8517_read_i2c_nonblock(client, 0x1c, 2, reg);
		if (ret < 0) {
			msleep(10);
			continue;
		} else {
			break;
		}
	}

	if (ret >= 0) {
		return true;
	} else {
		return false;
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0))
static int sc8517_charger_probe(struct i2c_client *client)
#else
static int sc8517_charger_probe(struct i2c_client *client,
                                const struct i2c_device_id *id)
#endif
{
	int ret;
	struct sc8517_device *chip;
	struct oplus_voocphy_manager *voocphy;

	if (!sc8517_check_device_is_exist(client)) {
		return 0;
	}

	chip = devm_kzalloc(&client->dev, sizeof(struct sc8517_device), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}

	voocphy = devm_kzalloc(&client->dev, sizeof(struct oplus_voocphy_manager), GFP_KERNEL);
	if (voocphy == NULL) {
		ret = -ENOMEM;
		goto chg_err;
	}

	chip->client = client;
	chip->dev = &client->dev;
	voocphy->client = client;
	voocphy->dev = &client->dev;
	voocphy->priv_data = chip;
	chip->voocphy = voocphy;
	mutex_init(&chip->i2c_rw_lock);
	mutex_init(&chip->chip_lock);
	i2c_set_clientdata(client, voocphy);

	sc8517_create_device_node(&(client->dev));
	voocphy->ops = &oplus_sc8517_ops;
	sc8517_dump_registers(voocphy);
	sc8517_parse_dt(voocphy);
	ret = oplus_register_voocphy(voocphy);
	if (ret < 0) {
		goto reg_voocphy_err;
	}
	ret = sc8517_irq_register(chip);
	if (ret < 0) {
		goto reg_irq_err;
	}

	ret = sc8517_ic_register(chip);
	if (ret < 0) {
		goto cp_reg_err;
	}

	sc8517_cp_init(chip->cp_ic);

	sc8517_dump_registers(voocphy);
	register_voocphy_devinfo();
	sc8517_set_vac2v2x_uvp(voocphy, false);

	return 0;

cp_reg_err:
	free_irq(voocphy->irq, voocphy);
reg_irq_err:
	gpio_free(voocphy->irq_gpio);
reg_voocphy_err:
	devm_kfree(&client->dev, voocphy);
chg_err:
	devm_kfree(&client->dev, chip);
	return ret;
}

static void sc8517_charger_shutdown(struct i2c_client *client)
{
	sc8517_update_bits(client, SC8517_REG_06, SC8517_REG_RESET_MASK,
			   SC8517_RESET_REG << SC8517_REG_RESET_SHIFT);
	msleep(10);
	/* disable v1x_scp */
	sc8517_write_byte(client, SC8517_REG_01, 0x5e);
	/* REF_SKIP_R 40mv */
	sc8517_write_byte(client, SC8517_REG_08, 0xA6);
	/* Masked Pulse_filtered, RX_Start,Tx_Done,bit soft intflag */
	sc8517_write_byte(client, SC8517_REG_29, 0x05);

	sc8517_update_bits(client, SC8517_REG_7D, SC8517_CHG_DIS_VAC2V2X_UV_MASK,
			    SC8517_CHG_DIS_VAC2V2X_UV_EN << SC8517_CHG_DIS_VAC2V2X_UV_SHIFT);

	return;
}

static const struct i2c_device_id sc8517_charger_id[] = {
	{"sc8517-master", 0},
	{},
};

static struct i2c_driver sc8517_charger_driver = {
	.driver		= {
		.name	= "sc8517-charger",
		.owner	= THIS_MODULE,
		.of_match_table = sc8517_charger_match_table,
	},
	.id_table	= sc8517_charger_id,

	.probe		= sc8517_charger_probe,
	.shutdown	= sc8517_charger_shutdown,
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
static int __init sc8517_subsys_init(void)
{
	int ret = 0;

	if (i2c_add_driver(&sc8517_charger_driver) != 0) {
	} else {
	}

	return ret;
}

subsys_initcall(sc8517_subsys_init);
#else
static int sc8517_subsys_init(void)
{
	int ret = 0;

	if (i2c_add_driver(&sc8517_charger_driver) != 0) {
	} else {
	}

	return ret;
}

static void sc8517_subsys_exit(void)
{
	i2c_del_driver(&sc8517_charger_driver);
}
oplus_chg_module_register(sc8517_subsys);
#endif /*LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)*/

MODULE_DESCRIPTION("SC SC8517 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
