// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "[HAL_WLS]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/list.h>
#include <linux/kthread.h>

#include <oplus_chg.h>
#include <oplus_chg_module.h>
#include <oplus_chg_ic.h>
#include <oplus_chg_monitor.h>
#include <oplus_mms.h>
#include <oplus_mms_wired.h>

/**********************************************************************
* wls rx APIs:
**********************************************************************/
void oplus_chg_wls_rx_dump_regs(struct oplus_chg_ic_dev *rx_ic)
{
	int rc;

	if (!rx_ic) {
		return;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_REG_DUMP);
}

#ifdef USE_DEFAULT_SMT_TEST
int oplus_chg_wls_rx_smt_test(struct oplus_chg_ic_dev *rx_ic, char buf[], int len)
{
	int rc;

	if (!rx_ic) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_SMT_TEST, buf, len);

	return rc;
}
#endif

int oplus_chg_wls_rx_enable(struct oplus_chg_ic_dev *rx_ic, bool en)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_ENABLE, en);

	return rc;
}

bool oplus_chg_wls_rx_is_enable(struct oplus_chg_ic_dev *rx_ic)
{
	int rc;
	bool enable;

	if (rx_ic == NULL) {
		return false;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_IS_ENABLE, &enable);
	if (rc < 0) {
		return false;
	}

	return enable;
}

bool oplus_chg_wls_rx_is_connected(struct oplus_chg_ic_dev *rx_ic)
{
	int rc;
	bool connected;

	if (rx_ic == NULL) {
		return false;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_IS_CONNECTED, &connected);
	if (rc < 0) {
		return false;
	}

	return connected;
}

int oplus_chg_wls_rx_get_vout(struct oplus_chg_ic_dev *rx_ic, int *vol_mv)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_VOUT, vol_mv);

	return rc;
}

int oplus_chg_wls_rx_set_vout_raw(struct oplus_chg_ic_dev *rx_ic, int vol_mv)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_VOUT, vol_mv);

	return rc;
}

int oplus_chg_wls_rx_get_vrect(struct oplus_chg_ic_dev *rx_ic, int *vol_mv)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_VRECT, vol_mv);

	return rc;
}

int oplus_chg_wls_rx_get_iout(struct oplus_chg_ic_dev *rx_ic, int *curr_ma)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_IOUT, curr_ma);

	return rc;
}

int oplus_chg_wls_rx_get_trx_vol(struct oplus_chg_ic_dev *rx_ic, int *vol_mv)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_TRX_VOL, vol_mv);

	return rc;
}

int oplus_chg_wls_rx_get_trx_curr(struct oplus_chg_ic_dev *rx_ic, int *curr_ma)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_TRX_CURR, curr_ma);

	return rc;
}

int oplus_chg_wls_get_cep(struct oplus_chg_ic_dev *rx_ic, int *cep)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_CEP_VAL, cep);

	return rc;
}

int oplus_chg_wls_get_cep_check_update(struct oplus_chg_ic_dev *rx_ic, int *cep)
{
	static int cep_count;
	int cep_count_temp;
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_CEP_COUNT, &cep_count_temp);
	if (rc < 0) {
		return rc;
	}
	if (cep_count == cep_count_temp) {
		return -EINVAL;
	}
	cep_count = cep_count_temp;
	rc = oplus_chg_wls_get_cep(rx_ic, cep);

	return rc;
}

int oplus_chg_wls_rx_get_work_freq(struct oplus_chg_ic_dev *rx_ic, int *freq)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_WORK_FREQ, freq);

	return rc;
}

int oplus_chg_wls_rx_get_rx_mode(struct oplus_chg_ic_dev *rx_ic, enum oplus_chg_wls_rx_mode *rx_mode)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_RX_MODE, rx_mode);

	return rc;
}

int oplus_chg_wls_rx_set_rx_mode(struct oplus_chg_ic_dev *rx_ic, enum oplus_chg_wls_rx_mode rx_mode)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_RX_MODE, rx_mode);

	return rc;
}

int oplus_chg_wls_rx_set_dcdc_enable(struct oplus_chg_ic_dev *rx_ic, bool en)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_DCDC_ENABLE, en);

	return rc;
}

int oplus_chg_wls_rx_set_trx_enable(struct oplus_chg_ic_dev *rx_ic, bool en)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_TRX_ENABLE, en);

	return rc;
}

int oplus_chg_wls_rx_set_trx_start(struct oplus_chg_ic_dev *rx_ic, enum oplus_chg_wls_tx_start_type start)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_TRX_START, start);

	return rc;
}

int oplus_chg_wls_rx_get_trx_status(struct oplus_chg_ic_dev *rx_ic, u8 *status)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_TRX_STATUS, status);
	return rc;
}

int oplus_chg_wls_rx_get_trx_err(struct oplus_chg_ic_dev *rx_ic, u32 *err)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_TRX_ERR, err);

	return rc;
}

int oplus_chg_wls_get_headroom(struct oplus_chg_ic_dev *rx_ic, int *val)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_HEADROOM, val);

	return rc;
}

int oplus_chg_wls_set_headroom(struct oplus_chg_ic_dev *rx_ic, int val)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_HEADROOM, val);

	return rc;
}

int oplus_chg_wls_rx_send_match_q(struct oplus_chg_ic_dev *rx_ic, u8 data[])
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SEND_MATCH_Q, data);

	return rc;
}

int oplus_chg_wls_rx_set_fod_parm(struct oplus_chg_ic_dev *rx_ic, u8 buf[], int len, int mode, int magcvr)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_FOD_PARM, buf, len, mode, magcvr);

	return rc;
}

int oplus_chg_wls_rx_send_epp_match_q(struct oplus_chg_ic_dev *rx_ic, u8 data[])
{
	int rc;

	if (rx_ic == NULL || data == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SEND_EPP_MATCH_Q, data);

	return rc;
}

int oplus_chg_wls_rx_send_msg_raw(struct oplus_chg_ic_dev *rx_ic, u8 buf[], int len, int raw_data)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SEND_MSG, buf, len, raw_data);

	return rc;
}

int oplus_chg_wls_rx_register_msg_callback(struct oplus_chg_ic_dev *rx_ic, void *data, void (*call_back)(void *, u8[]))
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_REG_MSG_CALLBACK, data, call_back);

	return rc;
}

int oplus_chg_wls_rx_get_rx_version(struct oplus_chg_ic_dev *rx_ic, u32 *version)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_RX_VERSION, version);

	return rc;
}

int oplus_chg_wls_rx_get_trx_version(struct oplus_chg_ic_dev *rx_ic, u32 *version)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_TRX_VERSION, version);

	return rc;
}

int oplus_chg_wls_rx_upgrade_firmware_by_buf_raw(struct oplus_chg_ic_dev *rx_ic, unsigned char *buf, int len)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_UPGRADE_FW_BY_BUF, buf, len);

	return rc;
}

int oplus_chg_wls_rx_upgrade_firmware_by_img_raw(struct oplus_chg_ic_dev *rx_ic)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_UPGRADE_FW_BY_IMG);

	return rc;
}

int oplus_chg_wls_rx_connect_check(struct oplus_chg_ic_dev *rx_ic)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_CHECK_CONNECT);

	return rc;
}

int oplus_chg_wls_rx_get_event_code(struct oplus_chg_ic_dev *rx_ic, enum oplus_chg_wls_event_code *code)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_EVENT_CODE, code);

	return rc;
}

int oplus_chg_wls_rx_get_bridge_mode(struct oplus_chg_ic_dev *rx_ic, int *mode)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_BRIDGE_MODE, mode);

	return rc;
}

int oplus_chg_wls_rx_set_insert_disable(struct oplus_chg_ic_dev *rx_ic, bool en)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_DIS_INSERT, en);

	return rc;
}

int oplus_chg_wls_rx_standby_config(struct oplus_chg_ic_dev *rx_ic, bool en)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_STANDBY_CONFIG, en);

	return rc;
}

int oplus_chg_wls_rx_set_comu(struct oplus_chg_ic_dev *rx_ic, int comu)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_COMU, comu);

	return rc;
}

int oplus_chg_wls_rx_get_tx_id(struct oplus_chg_ic_dev *rx_ic, int *tx_id)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_TX_ID, tx_id);

	return rc;
}

int oplus_chg_wls_rx_set_silent(struct oplus_chg_ic_dev *rx_ic)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_SET_SILENT);

	return rc;
}

int oplus_chg_wls_rx_get_ble_mac_addr(struct oplus_chg_ic_dev *rx_ic, u64 *mac_addr)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_BLE_MAC_ADDR, mac_addr);

	return rc;
}

int oplus_chg_wls_rx_get_wlspen_id(struct oplus_chg_ic_dev *rx_ic, u8 *wlspen_id)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_WLSPEN_ID, wlspen_id);

	return rc;
}

int oplus_chg_wls_rx_get_wlspen_chg_status(struct oplus_chg_ic_dev *rx_ic, u8 *chg_status)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_WLSPEN_CHG_STATUS, chg_status);

	return rc;
}

int oplus_chg_wls_rx_get_ac_ov_flag(struct oplus_chg_ic_dev *rx_ic, int *ac_ov_flag)
{
	int rc;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(rx_ic, OPLUS_IC_FUNC_RX_GET_AC_OV_FLAG, ac_ov_flag);

	return rc;
}

int oplus_chg_wls_rx_smt_test(struct oplus_chg_ic_dev *rx_ic)
{
	int rc;
	u32 err_code = 0;
	u32 version;

	if (rx_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_wls_rx_get_rx_version(rx_ic, &version);
	if (rc < 0) {
		err_code |= BIT(rx_ic->index);
	}

	return err_code;
}

/**********************************************************************
* wls nor APIs:
**********************************************************************/
__maybe_unused static bool oplus_chg_is_usb_present(void)
{
	struct oplus_mms *wired_topic;
	union mms_msg_data data = { 0 };
	int rc;

	wired_topic = oplus_mms_get_by_name("wired");
	if (!wired_topic)
		return false;

	rc = oplus_mms_get_item_data(wired_topic, WIRED_ITEM_PRESENT, &data, true);
	if (rc < 0)
		return false;

	return !!data.intval;
}

int oplus_chg_wls_nor_set_input_enable(struct oplus_chg_ic_dev *nor_ic, bool en)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_WLS_INPUT_SUSPEND, !en);
	if (rc < 0)
		;
	else
		;

	return rc;
}

int oplus_chg_wls_nor_set_output_enable(struct oplus_chg_ic_dev *nor_ic, bool en)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}
	if (oplus_chg_is_usb_present()) {
		return -EPERM;
	}
	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_OUTPUT_SUSPEND, !en);
	if (rc < 0)
		;
	else
		;

	return rc;
}

int oplus_chg_wls_nor_set_icl_raw(struct oplus_chg_ic_dev *nor_ic, int icl_ma)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_SET_WLS_ICL, icl_ma);

	return rc;
}

int oplus_chg_wls_nor_set_fcc_raw(struct oplus_chg_ic_dev *nor_ic, int fcc_ma)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_SET_FCC, fcc_ma);

	return rc;
}

int oplus_chg_wls_nor_set_fv(struct oplus_chg_ic_dev *nor_ic, int fv_mv)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_SET_FV, fv_mv);
	if (rc < 0) {
		return rc;
	} else {
	}

	return 0;
}

int oplus_chg_wls_nor_set_rechg_vol(struct oplus_chg_ic_dev *nor_ic, int rechg_vol_mv)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_SET_RECHG_VOL, rechg_vol_mv);
	if (rc < 0) {
		return rc;
	} else {
	}

	return 0;
}

int oplus_chg_wls_nor_get_icl(struct oplus_chg_ic_dev *nor_ic, int *icl_ma)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_GET_WLS_ICL, icl_ma);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_get_input_curr(struct oplus_chg_ic_dev *nor_ic, int *curr_ma)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_GET_WLS_INPUT_CURR, curr_ma);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_get_input_vol(struct oplus_chg_ic_dev *nor_ic, int *vol_mv)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_GET_WLS_INPUT_VOL, vol_mv);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_set_boost_en(struct oplus_chg_ic_dev *nor_ic, bool en)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_WLS_BOOST_ENABLE, en);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_set_boost_vol(struct oplus_chg_ic_dev *nor_ic, int vol_mv)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_SET_WLS_BOOST_VOL, vol_mv);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_set_boost_curr_limit(struct oplus_chg_ic_dev *nor_ic, int curr_ma)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}


	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_SET_WLS_BOOST_CURR_LIMIT, curr_ma);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_set_aicl_enable(struct oplus_chg_ic_dev *nor_ic, bool en)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_WLS_AICL_ENABLE, en);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_set_aicl_rerun(struct oplus_chg_ic_dev *nor_ic)
{
	int rc;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_WLS_AICL_RERUN);
	if (rc < 0) {
		return rc;
	}

	return 0;
}

int oplus_chg_wls_nor_set_vindpm(struct oplus_chg_ic_dev *nor_ic, int vindpm_mv)
{
	int rc = 0;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_SET_VINDPM, vindpm_mv);

	return rc;
}

int oplus_chg_wls_nor_set_usb_drv(struct oplus_chg_ic_dev *nor_ic, bool en)
{
	int rc = 0;

	if (nor_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(nor_ic, OPLUS_IC_FUNC_BUCK_DIS_INSERT_DETECT, en);

	return rc;
}

/**********************************************************************
* wls fast APIs:
**********************************************************************/
int oplus_chg_wls_fast_set_enable(struct oplus_chg_ic_dev *fast_ic, bool en)
{
	int rc;

	if (fast_ic == NULL) {
		return -ENODEV;
	}

	if (!en) {
		rc = oplus_chg_ic_func(fast_ic, OPLUS_IC_FUNC_CP_SET_WORK_START, false);
	} else {
		rc = oplus_chg_ic_func(fast_ic, OPLUS_IC_FUNC_CP_SET_WORK_MODE, CP_WORK_MODE_2_TO_1);
	}

	rc = oplus_chg_ic_func(fast_ic, OPLUS_IC_FUNC_CP_ENABLE, en);

	return rc;
}

int oplus_chg_wls_fast_start(struct oplus_chg_ic_dev *fast_ic)
{
	int rc;

	if (fast_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(fast_ic, OPLUS_IC_FUNC_CP_SET_WORK_START, true);

	return rc;
}

int oplus_chg_wls_fast_smt_test(struct oplus_chg_ic_dev *fast_ic)
{
	int rc;

	if (fast_ic == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(fast_ic, OPLUS_IC_FUNC_CP_SET_WORK_START, true);

	return rc;
}

int oplus_chg_wls_fast_get_fault(struct oplus_chg_ic_dev *fast_ic, char *fault)
{
	int rc = 0;

	if (fast_ic == NULL) {
		return -ENODEV;
	}

	if (fault == NULL) {
		return -ENODEV;
	}
	/*TODO*/

	return rc;
}
