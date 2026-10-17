#define pr_fmt(fmt) "[VIRTUAL_VOOCPHY]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/list.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
#include <soc/oplus/system/boot_mode.h>
#include <soc/oplus/device_info.h>
#include <soc/oplus/system/oplus_project.h>
#endif
#include <oplus_chg.h>
#include <oplus_chg_module.h>
#include <oplus_chg_ic.h>
#include <oplus_hal_vooc.h>
#include <oplus_mms_gauge.h>
#include <oplus_chg_monitor.h>

struct oplus_virtual_vphy_gpio {
	int switch_ctrl_gpio;
	int vphy_switch1_gpio;
	int vphy_switch2_gpio;
	int vphy_id_gpio;
	int data_irq;

	struct pinctrl *pinctrl;
	struct mutex pinctrl_mutex;

	struct pinctrl_state *vphy_switch_normal;
	struct pinctrl_state *vphy_switch_vooc;
	struct pinctrl_state *gpio_switch_ctrl_ap;
	struct pinctrl_state *gpio_switch_ctrl_vphy;

	struct pinctrl_state *vphy_id_active;
	struct pinctrl_state *vphy_id_sleep;
};

struct oplus_virtual_vphy_child {
	struct oplus_chg_ic_dev *ic_dev;
	bool initialized;
};

struct oplus_virtual_vphy_ic {
	struct device *dev;
	struct oplus_chg_ic_dev *ic_dev;
	struct oplus_chg_ic_dev *vphy;
	struct oplus_chg_ic_dev *dpdm_switch;
	struct oplus_virtual_vphy_child *vphy_list;
	struct oplus_virtual_vphy_gpio gpio;
	int vphy_num;

	enum oplus_chg_vooc_ic_type vooc_ic_type;
	enum oplus_chg_vooc_switch_mode switch_mode;

	struct work_struct data_handler_work;
	struct delayed_work upload_device_id_work;

	bool use_dpdm_switch_ic;
};


static struct oplus_chg_ic_virq oplus_vphy_virq_table[] = {
	{ .virq_id = OPLUS_IC_VIRQ_ERR },
	{ .virq_id = OPLUS_IC_VIRQ_VOOC_DATA },
};

static void oplus_vphy_upload_device_id_subscribe_error_topic(
	struct oplus_mms *topic, void *prv_data)
{
	struct oplus_virtual_vphy_ic *chip = prv_data;

	/* delay 50ms upload for error topic init stop publish finish */
	schedule_delayed_work(&chip->upload_device_id_work, msecs_to_jiffies(50));
}

static void oplus_vphy_upload_device_id_work(struct work_struct *work)
{
	int rc;
	struct mms_msg *msg;
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_virtual_vphy_ic *chip = container_of(dwork,
		struct oplus_virtual_vphy_ic, upload_device_id_work);
	struct oplus_mms *err_topic = oplus_mms_get_by_name("error");

	if (!err_topic)
		return;

	if (!chip->vphy)
		return;

	msg = oplus_mms_alloc_str_msg(MSG_TYPE_ITEM,
		MSG_PRIO_MEDIUM, ERR_ITEM_DEVICE_ID, "vooc_ic=%s", chip->vphy->name);
	if (msg == NULL) {
		return;
	}

	rc = oplus_mms_publish_msg_sync(err_topic, msg);
	if (rc < 0) {
		kfree(msg);
	}
}

static void oplus_vphy_upload_device_id(struct oplus_virtual_vphy_ic *chip)
{
	oplus_mms_wait_topic("error",
		oplus_vphy_upload_device_id_subscribe_error_topic, chip);
}

static void oplus_vphy_err_handler(struct oplus_chg_ic_dev *ic_dev, void *virq_data)
{
	struct oplus_virtual_vphy_ic *chip = virq_data;

	oplus_chg_ic_move_err_msg(chip->ic_dev, ic_dev);
	oplus_chg_ic_virq_trigger(chip->ic_dev, OPLUS_IC_VIRQ_ERR);
}

static void oplus_vphy_data_handler(struct oplus_chg_ic_dev *ic_dev, void *virq_data)
{
	struct oplus_virtual_vphy_ic *chip = virq_data;

	oplus_chg_ic_virq_trigger(chip->ic_dev, OPLUS_IC_VIRQ_VOOC_DATA);
}

static int oplus_vphy_virq_register(struct oplus_virtual_vphy_ic *chip)
{
	int rc;
	int i;

	if (chip->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_virq_register(chip->vphy,
		OPLUS_IC_VIRQ_ERR, oplus_vphy_err_handler, chip);

	for (i = 0; i < chip->vphy_num; i++) {
		rc = oplus_chg_ic_virq_register(chip->vphy_list[i].ic_dev,
						OPLUS_IC_VIRQ_VOOC_DATA,
						oplus_vphy_data_handler, chip);
	};

	return 0;
}

static int oplus_chg_vphy_init(struct oplus_chg_ic_dev *ic_dev)
{
	struct oplus_virtual_vphy_ic *va;
	struct device_node *node;
	bool retry = false;
	int i;
	int rc = 0;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	node = va->dev->of_node;

	if (va->use_dpdm_switch_ic) {
		va->dpdm_switch = of_get_oplus_chg_ic(node, "oplus,dpdm_switch_ic", 0);
		if (va->dpdm_switch == NULL) {
			return -EAGAIN;
		}
	}

	ic_dev->online = true;
	for (i = 0; i < va->vphy_num; i++) {
		if (va->vphy_list[i].initialized)
			continue;
		va->vphy_list[i].ic_dev =
			of_get_oplus_chg_ic(node, "oplus,vphy_ic", i);
		if (va->vphy_list[i].ic_dev == NULL) {
			retry = true;
			continue;
		}
		oplus_chg_ic_set_parent(va->vphy_list[i].ic_dev, ic_dev);
		rc = oplus_chg_ic_func(va->vphy_list[i].ic_dev, OPLUS_IC_FUNC_INIT);
		va->vphy_list[i].initialized = true;
		if (rc >= 0) {
			va->vphy = va->vphy_list[i].ic_dev;
			va->vooc_ic_type = rc;
			goto init_done;
		} else {
		}
	}

	if (retry) {
		return -EAGAIN;
	} else {
		return -EINVAL;
	}

init_done:
	oplus_vphy_virq_register(va);
	oplus_vphy_upload_device_id(va);
	return rc;
}

static int oplus_chg_vphy_exit(struct oplus_chg_ic_dev *ic_dev)
{
	struct oplus_virtual_vphy_ic *va;
	int i;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	if (!ic_dev->online)
		return 0;

	va = oplus_chg_ic_get_drvdata(ic_dev);
	ic_dev->online = false;

	oplus_chg_ic_virq_release(va->vphy, OPLUS_IC_VIRQ_ERR, va);
	for (i = 0; i < va->vphy_num; i++) {
		oplus_chg_ic_virq_release(va->vphy_list[i].ic_dev,
					  OPLUS_IC_VIRQ_VOOC_DATA, va);
	};

	oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_EXIT);
	oplus_chg_ic_set_parent(ic_dev, NULL);

	return 0;
}

static int oplus_chg_vvphy_reply_data(struct oplus_chg_ic_dev *ic_dev,
				      int data, int data_width, int curr_ma)
{
	struct oplus_virtual_vphy_ic *vvphy;
	int rc = 0;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	vvphy = oplus_chg_ic_get_drvdata(ic_dev);
	if (vvphy->vphy == NULL) {
		return -ENODEV;
	}
	rc = oplus_chg_ic_func(vvphy->vphy, OPLUS_IC_FUNC_VOOC_REPLY_DATA,
			       data, data_width, curr_ma);

	return rc;
}

static int oplus_chg_vvphy_get_data(struct oplus_chg_ic_dev *ic_dev, int *data)
{
	struct oplus_virtual_vphy_ic *vvphy;
	int rc = 0;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	vvphy = oplus_chg_ic_get_drvdata(ic_dev);
	if (vvphy->vphy == NULL) {
		return -ENODEV;
	}
	rc = oplus_chg_ic_func(vvphy->vphy, OPLUS_IC_FUNC_VOOC_READ_DATA, data);

	return rc;
}

static int oplus_chg_vphy_set_switch_mode(struct oplus_chg_ic_dev *ic_dev,
					int mode)
{
	struct oplus_virtual_vphy_ic *va;
	int retry = 10;
	int rc = 0;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);

	switch (mode) {
	case VOOC_SWITCH_MODE_VOOC:
		do {
			if (va->use_dpdm_switch_ic) {
				rc = oplus_chg_ic_func(va->dpdm_switch,
					OPLUS_IC_FUNC_SET_DPDM_SWITCH_MODE, DPDM_SWITCH_TO_VOOC);
				if (rc < 0) {
					goto err;
				}
			} else {
				if (IS_ERR_OR_NULL(va->gpio.pinctrl)) {
					goto exit;
				}

				mutex_lock(&va->gpio.pinctrl_mutex);
				rc = pinctrl_select_state(
					va->gpio.pinctrl,
					va->gpio.vphy_switch_vooc);
				if (rc < 0) {
					goto err;
				}
				rc = pinctrl_select_state(
					va->gpio.pinctrl,
					va->gpio.gpio_switch_ctrl_vphy);
				if (rc < 0) {
					goto err;
				}
				mutex_unlock(&va->gpio.pinctrl_mutex);
			}
			break;
			usleep_range(5000, 5000);
		} while (--retry > 0);
		break;
	case VOOC_SWITCH_MODE_HEADPHONE:
		;
		break;
	case VOOC_SWITCH_MODE_NORMAL:
	default:
		if (va->use_dpdm_switch_ic) {
			rc = oplus_chg_ic_func(va->dpdm_switch,
				OPLUS_IC_FUNC_SET_DPDM_SWITCH_MODE, DPDM_SWITCH_TO_AP);
			if (rc < 0) {
				goto err;
			}
		} else {
			if (IS_ERR_OR_NULL(va->gpio.pinctrl)) {
				goto exit;
			}

			mutex_lock(&va->gpio.pinctrl_mutex);
			rc = pinctrl_select_state(va->gpio.pinctrl,
						  va->gpio.vphy_switch_normal);
			if (rc < 0) {
				goto err;
			}
			rc = pinctrl_select_state(va->gpio.pinctrl,
						va->gpio.gpio_switch_ctrl_ap);
			if (rc < 0) {
				goto err;
			}
			mutex_unlock(&va->gpio.pinctrl_mutex);
		}
		break;
	}

	va->switch_mode = mode;
exit:
	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOC_SET_SWITCH_MODE, mode);
	if (rc < 0) {
		goto err;
	}

	return 0;
err:
	if (!va->use_dpdm_switch_ic) {
		pinctrl_select_state(va->gpio.pinctrl, va->gpio.vphy_switch_normal);
		pinctrl_select_state(va->gpio.pinctrl, va->gpio.gpio_switch_ctrl_ap);
		mutex_unlock(&va->gpio.pinctrl_mutex);
	} else {
		oplus_chg_ic_func(va->dpdm_switch,
			OPLUS_IC_FUNC_SET_DPDM_SWITCH_MODE, DPDM_SWITCH_TO_AP);
	}
	return rc;
}

static int oplus_chg_vphy_get_switch_mode(struct oplus_chg_ic_dev *ic_dev,
					int *mode)
{
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);
	*mode = va->switch_mode;

	return 0;
}

static int oplus_chg_vphy_reset_sleep(struct oplus_chg_ic_dev *ic_dev)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOC_RESET_SLEEP);

	return 0;
}

static int oplus_chg_vphy_set_pdqc_config(struct oplus_chg_ic_dev *ic_dev)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_SET_PDQC_CONFIG);

	return rc;
}

static int oplus_chg_vphy_set_usb_dischg_enable(struct oplus_chg_ic_dev *ic_dev, bool enable)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if ((va == NULL) || (va->vphy == NULL)) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_SET_USB_DISCHG_ENABLE, enable);

	return rc;
}

static int oplus_chg_vphy_get_cp_vbat(struct oplus_chg_ic_dev *ic_dev,
				      int *cp_vbat)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_CP_VBAT,
			       cp_vbat);

	return rc;
}

static int oplus_chg_vphy_set_bcc_curr(struct oplus_chg_ic_dev *ic_dev,
				      int *cp_data)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	if (cp_data == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_SET_BCC_CURR,
			       cp_data);

	return rc;
}

static int oplus_chg_vphy_set_ucp_time(struct oplus_chg_ic_dev *ic_dev, int value)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_SET_UCP_TIME,
			       value);

	return rc;
}


static int oplus_chg_vphy_get_bcc_max_curr(struct oplus_chg_ic_dev *ic_dev,
				      int *cp_data)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MAX_CURR,
			       cp_data);

	return rc;
}

static int oplus_chg_vphy_get_bcc_min_curr(struct oplus_chg_ic_dev *ic_dev,
				      int *cp_data)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MIN_CURR,
			       cp_data);

	return rc;
}

static int oplus_chg_vphy_get_bcc_exit_curr(struct oplus_chg_ic_dev *ic_dev,
				      int *cp_data)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_BCC_EXIT_CURR,
			       cp_data);

	return rc;
}

static int oplus_chg_vphy_get_fastchg_ing(struct oplus_chg_ic_dev *ic_dev,
				      int *cp_data)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_FASTCHG_ING,
			       cp_data);

	return rc;
}

static int oplus_chg_vphy_get_bcc_temp_range(struct oplus_chg_ic_dev *ic_dev,
				      int *cp_data)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_BCC_TEMP_RANGE,
			       cp_data);

	return rc;
}

static int oplus_chg_vphy_set_chg_auto_mode(struct oplus_chg_ic_dev *ic_dev,
					    bool enable)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy,
			       OPLUS_IC_FUNC_VOOCPHY_SET_CHG_AUTO_MODE,
			       enable);

	return rc;
}

static int oplus_chg_vphy_set_vac2v2x_uvp(struct oplus_chg_ic_dev *ic_dev,
					 bool disable)
{
	int rc;
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	if (va->vphy == NULL) {
		return -ENODEV;
	}

	rc = oplus_chg_ic_func(va->vphy,
			       OPLUS_IC_FUNC_VOOCPHY_SET_VAC2V2X_UVP,
			       disable);

	return rc;
}

static int oplus_chg_vphy_get_vphy(struct oplus_chg_ic_dev *ic_dev,
				   struct oplus_chg_ic_dev **vphy)
{
	struct oplus_virtual_vphy_ic *va;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	va = oplus_chg_ic_get_drvdata(ic_dev);
	*vphy = va->vphy;

	return 0;
}

static int oplus_chg_vphy_get_curve_current(struct oplus_chg_ic_dev *ic_dev, int *curr)
{
	struct oplus_virtual_vphy_ic *va;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOC_GET_CURVE_CURR, curr);

	return rc;
}

static int oplus_chg_vphy_get_real_curve_current(struct oplus_chg_ic_dev *ic_dev, int *curr)
{
	struct oplus_virtual_vphy_ic *va;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOC_GET_REAL_CURVE_CURR, curr);

	return rc;
}

static int oplus_chg_vphy_set_ap_fastchg_allow(struct oplus_chg_ic_dev *ic_dev, int allow, bool dummy)
{
	struct oplus_virtual_vphy_ic *va;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);
	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_SET_AP_FASTCHG_ALLOW, allow, dummy);

	return rc;
}

static int oplus_chg_vphy_get_retry_flag(struct oplus_chg_ic_dev *ic_dev, bool *retry_flag)
{
	struct oplus_virtual_vphy_ic *va;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);

	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_RETRY_FLAG, retry_flag);

	return rc;
}

static int oplus_chg_vphy_set_shutdown_switch_mode(struct oplus_chg_ic_dev *ic_dev)
{
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	rc = oplus_chg_vphy_reset_sleep(ic_dev);
	rc = oplus_chg_vphy_set_switch_mode(ic_dev, VOOC_SWITCH_MODE_NORMAL);
	msleep(500);

	return rc;
}

static int oplus_chg_vphy_get_frame_head(struct oplus_chg_ic_dev *ic_dev, int *head)
{
	struct oplus_virtual_vphy_ic *va;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);
	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_FRAME_HEAD, head);

	return rc;
}

static int oplus_chg_vphy_get_fastchg_commu_ing(struct oplus_chg_ic_dev *ic_dev,
				   bool *fastchg_commu_ing)
{
	struct oplus_virtual_vphy_ic *va;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}
	va = oplus_chg_ic_get_drvdata(ic_dev);
	rc = oplus_chg_ic_func(va->vphy, OPLUS_IC_FUNC_VOOCPHY_GET_FASTCHG_COMMU_ING,
				   fastchg_commu_ing);

	return rc;
}

static void *oplus_chg_vphy_get_func(struct oplus_chg_ic_dev *ic_dev,
				   enum oplus_chg_ic_func func_id)
{
	void *func = NULL;

	if (!ic_dev->online && (func_id != OPLUS_IC_FUNC_INIT) &&
	    (func_id != OPLUS_IC_FUNC_EXIT)) {
		return NULL;
	}

	switch (func_id) {
	case OPLUS_IC_FUNC_INIT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_INIT,
					       oplus_chg_vphy_init);
		break;
	case OPLUS_IC_FUNC_EXIT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_EXIT,
					       oplus_chg_vphy_exit);
		break;
	case OPLUS_IC_FUNC_VOOC_READ_DATA:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOC_READ_DATA,
					       oplus_chg_vvphy_get_data);
		break;
	case OPLUS_IC_FUNC_VOOC_REPLY_DATA:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOC_REPLY_DATA,
					       oplus_chg_vvphy_reply_data);
		break;
	case OPLUS_IC_FUNC_VOOC_SET_SWITCH_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(
			OPLUS_IC_FUNC_VOOC_SET_SWITCH_MODE,
			oplus_chg_vphy_set_switch_mode);
		break;
	case OPLUS_IC_FUNC_VOOC_GET_SWITCH_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(
			OPLUS_IC_FUNC_VOOC_GET_SWITCH_MODE,
			oplus_chg_vphy_get_switch_mode);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_SET_PDQC_CONFIG:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_SET_PDQC_CONFIG,
					       oplus_chg_vphy_set_pdqc_config);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_CP_VBAT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_CP_VBAT,
					       oplus_chg_vphy_get_cp_vbat);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_SET_BCC_CURR:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_SET_BCC_CURR,
					       oplus_chg_vphy_set_bcc_curr);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MAX_CURR:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MAX_CURR,
					       oplus_chg_vphy_get_bcc_max_curr);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MIN_CURR:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_BCC_MIN_CURR,
					       oplus_chg_vphy_get_bcc_min_curr);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_BCC_EXIT_CURR:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_BCC_EXIT_CURR,
					       oplus_chg_vphy_get_bcc_exit_curr);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_FASTCHG_ING:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_FASTCHG_ING,
					       oplus_chg_vphy_get_fastchg_ing);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_BCC_TEMP_RANGE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_BCC_TEMP_RANGE,
					       oplus_chg_vphy_get_bcc_temp_range);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_SET_CHG_AUTO_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_SET_CHG_AUTO_MODE,
					       oplus_chg_vphy_set_chg_auto_mode);
		break;
	case OPLUS_IC_FUNC_VOOC_GET_IC_DEV:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOC_GET_IC_DEV,
					       oplus_chg_vphy_get_vphy);
		break;
	case OPLUS_IC_FUNC_VOOC_GET_CURVE_CURR:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOC_GET_CURVE_CURR,
					       oplus_chg_vphy_get_curve_current);
		break;
	case OPLUS_IC_FUNC_VOOC_RESET_SLEEP:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOC_RESET_SLEEP,
					       oplus_chg_vphy_reset_sleep);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_SET_UCP_TIME:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_SET_UCP_TIME,
					       oplus_chg_vphy_set_ucp_time);
		break;
	case OPLUS_IC_FUNC_VOOC_GET_REAL_CURVE_CURR:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOC_GET_REAL_CURVE_CURR,
					       oplus_chg_vphy_get_real_curve_current);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_SET_AP_FASTCHG_ALLOW:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_SET_AP_FASTCHG_ALLOW,
					       oplus_chg_vphy_set_ap_fastchg_allow);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_RETRY_FLAG:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_RETRY_FLAG,
					       oplus_chg_vphy_get_retry_flag);
		break;
	case OPLUS_IC_FUNC_VOOC_SET_SHUTDOW_SWITCH_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOC_SET_SHUTDOW_SWITCH_MODE,
					       oplus_chg_vphy_set_shutdown_switch_mode);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_FRAME_HEAD:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_FRAME_HEAD,
					       oplus_chg_vphy_get_frame_head);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_GET_FASTCHG_COMMU_ING:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_GET_FASTCHG_COMMU_ING,
					       oplus_chg_vphy_get_fastchg_commu_ing);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_SET_VAC2V2X_UVP:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_SET_VAC2V2X_UVP,
					       oplus_chg_vphy_set_vac2v2x_uvp);
		break;
	case OPLUS_IC_FUNC_VOOCPHY_SET_USB_DISCHG_ENABLE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_VOOCPHY_SET_USB_DISCHG_ENABLE,
					       oplus_chg_vphy_set_usb_dischg_enable);
		break;
	case OPLUS_IC_FUNC_VOOC_FW_UPGRADE:
	case OPLUS_IC_FUNC_VOOC_USER_FW_UPGRADE:
	case OPLUS_IC_FUNC_VOOC_FW_CHECK_THEN_RECOVER:
	case OPLUS_IC_FUNC_VOOC_FW_CHECK_THEN_RECOVER_FIX:
	case OPLUS_IC_FUNC_VOOC_GET_FW_VERSION:
	case OPLUS_IC_FUNC_VOOC_EINT_REGISTER:
	case OPLUS_IC_FUNC_VOOC_EINT_UNREGISTER:
	case OPLUS_IC_FUNC_VOOC_SET_DATA_ACTIVE:
	case OPLUS_IC_FUNC_VOOC_SET_DATA_SLEEP:
	case OPLUS_IC_FUNC_VOOC_SET_CLOCK_ACTIVE:
	case OPLUS_IC_FUNC_VOOC_SET_CLOCK_SLEEP:
	case OPLUS_IC_FUNC_VOOC_GET_DATA_GPIO_VAL:
	case OPLUS_IC_FUNC_VOOC_GET_RESET_GPIO_VAL:
	case OPLUS_IC_FUNC_VOOC_GET_CLOCK_GPIO_VAL:
	case OPLUS_IC_FUNC_VOOC_RESET_ACTIVE:
	case OPLUS_IC_FUNC_VOOC_RESET_ACTIVE_FORCE:
	case OPLUS_IC_FUNC_VOOC_BOOT_BY_GPIO:
	case OPLUS_IC_FUNC_VOOC_UPGRADING:
	case OPLUS_IC_FUNC_VOOC_CHECK_FW_STATUS:
		func = NULL;
		break;
	default:
		;
		func = NULL;
		break;
	}

	return func;
}

static void oplus_vphy_data_handler_work(struct work_struct *work)
{
	struct oplus_virtual_vphy_ic *va = container_of(
		work, struct oplus_virtual_vphy_ic, data_handler_work);

	oplus_chg_ic_virq_trigger(va->ic_dev, OPLUS_IC_VIRQ_VOOC_DATA);
}

static int oplus_vphy_child_init(struct oplus_virtual_vphy_ic *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc = 0;

	rc = of_property_count_elems_of_size(node, "oplus,vphy_ic",
					     sizeof(u32));
	if (rc < 0) {
		return rc;
	}
	chip->vphy_num = rc;
	chip->vphy_list = devm_kzalloc(chip->dev,
				       sizeof(struct oplus_virtual_vphy_child) *
					       chip->vphy_num,
				       GFP_KERNEL);
	if (chip->vphy_list == NULL) {
		rc = -ENOMEM;
		return rc;
	}

	return 0;
}

static int oplus_vphy_gpio_init(struct oplus_virtual_vphy_ic *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct oplus_virtual_vphy_gpio *vphy_gpio = &chip->gpio;
	int rc = 0;

	if (chip->use_dpdm_switch_ic)
		return 0;

	vphy_gpio->pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(vphy_gpio->pinctrl)) {
		return -ENODEV;
	}

	vphy_gpio->switch_ctrl_gpio =
		of_get_named_gpio(node, "oplus,switch_ctrl-gpio", 0);
	if (!gpio_is_valid(vphy_gpio->switch_ctrl_gpio)) {
		return -ENODEV;
	}
	rc = gpio_request(vphy_gpio->switch_ctrl_gpio, "switch_ctrl-gpio");
	if (rc != 0) {
		return rc;
	}
	vphy_gpio->gpio_switch_ctrl_ap =
		pinctrl_lookup_state(vphy_gpio->pinctrl, "switch_ctrl_ap");
	if (IS_ERR_OR_NULL(vphy_gpio->gpio_switch_ctrl_ap)) {
		rc = -EINVAL;
		goto free_switch_ctrl_gpio;
	}
	vphy_gpio->gpio_switch_ctrl_vphy =
		pinctrl_lookup_state(vphy_gpio->pinctrl, "switch_ctrl_vphy");
	if (IS_ERR_OR_NULL(vphy_gpio->gpio_switch_ctrl_vphy)) {
		rc = -EINVAL;
		goto free_switch_ctrl_gpio;
	}
	rc = pinctrl_select_state(vphy_gpio->pinctrl,
				  vphy_gpio->gpio_switch_ctrl_ap);
	if (rc < 0) {
		goto free_switch_ctrl_gpio;
	}

	vphy_gpio->vphy_switch1_gpio =
		of_get_named_gpio(node, "oplus,vphy_switch1-gpio", 0);
	if (!gpio_is_valid(vphy_gpio->vphy_switch1_gpio)) {
		rc = -ENODEV;
		goto free_switch_ctrl_gpio;
	}
	rc = gpio_request(vphy_gpio->vphy_switch1_gpio, "vphy_switch1-gpio");
	if (rc != 0) {
		goto free_switch_ctrl_gpio;
	}
	vphy_gpio->vphy_switch2_gpio =
		of_get_named_gpio(node, "oplus,vphy_switch2-gpio", 0);
	if (!gpio_is_valid(vphy_gpio->vphy_switch2_gpio)) {
	} else {
		rc = gpio_request(vphy_gpio->vphy_switch2_gpio,
				  "vphy_switch2-gpio");
		if (rc != 0) {
			goto free_vphy_switch1_gpio;
		}
	}
	vphy_gpio->vphy_switch_normal =
		pinctrl_lookup_state(vphy_gpio->pinctrl, "vphy_switch_normal");
	if (IS_ERR_OR_NULL(vphy_gpio->vphy_switch_normal)) {
		rc = -EINVAL;
		goto free_vphy_switch2_gpio;
	}
	vphy_gpio->vphy_switch_vooc =
		pinctrl_lookup_state(vphy_gpio->pinctrl, "vphy_switch_vooc");
	if (IS_ERR_OR_NULL(vphy_gpio->vphy_switch_vooc)) {
		rc = -EINVAL;
		goto free_vphy_switch2_gpio;
	}
	rc = pinctrl_select_state(vphy_gpio->pinctrl,
				  vphy_gpio->vphy_switch_normal);
	if (rc < 0) {
		goto free_vphy_switch2_gpio;
	}

	mutex_init(&vphy_gpio->pinctrl_mutex);

	return 0;

free_vphy_switch2_gpio:
	if (gpio_is_valid(vphy_gpio->vphy_switch2_gpio))
		gpio_free(vphy_gpio->vphy_switch2_gpio);
free_vphy_switch1_gpio:
	if (gpio_is_valid(vphy_gpio->vphy_switch1_gpio))
		gpio_free(vphy_gpio->vphy_switch1_gpio);
free_switch_ctrl_gpio:
	if (gpio_is_valid(vphy_gpio->switch_ctrl_gpio))
		gpio_free(vphy_gpio->switch_ctrl_gpio);

	return rc;
}

static int oplus_virtual_vphy_probe(struct platform_device *pdev)
{
	struct oplus_virtual_vphy_ic *chip;
	struct device_node *node = pdev->dev.of_node;
	struct oplus_chg_ic_cfg ic_cfg = { 0 };
	enum oplus_chg_ic_type ic_type;
	int ic_index;
	int rc = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct oplus_virtual_vphy_ic),
			    GFP_KERNEL);
	if (chip == NULL) {
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	INIT_WORK(&chip->data_handler_work, oplus_vphy_data_handler_work);
	INIT_DELAYED_WORK(&chip->upload_device_id_work, oplus_vphy_upload_device_id_work);

	chip->use_dpdm_switch_ic = of_property_read_bool(node, "oplus,dpdm_switch_ic");

	rc = oplus_vphy_gpio_init(chip);

	rc = oplus_vphy_child_init(chip);
	if (rc < 0) {
		goto child_init_err;
	}

	rc = of_property_read_u32(node, "oplus,ic_type", &ic_type);
	if (rc < 0) {
		goto reg_ic_err;
	}

	rc = of_property_read_u32(node, "oplus,ic_index", &ic_index);
	if (rc < 0) {
		goto reg_ic_err;
	}

	ic_cfg.name = node->name;
	ic_cfg.index = ic_index;
	snprintf(ic_cfg.manu_name, OPLUS_CHG_IC_MANU_NAME_MAX - 1, "voocphy-virtual");
	snprintf(ic_cfg.fw_id, OPLUS_CHG_IC_FW_ID_MAX - 1, "0x00");
	ic_cfg.type = ic_type;
	ic_cfg.get_func = oplus_chg_vphy_get_func;
	ic_cfg.virq_data = oplus_vphy_virq_table;
	ic_cfg.virq_num = ARRAY_SIZE(oplus_vphy_virq_table);
	ic_cfg.of_node = node;
	chip->ic_dev =
		devm_oplus_chg_ic_register(chip->dev, &ic_cfg);
	if (!chip->ic_dev) {
		rc = -ENODEV;
		goto reg_ic_err;
	}

	return 0;

reg_ic_err:
	if (gpio_is_valid(chip->gpio.vphy_id_gpio))
		gpio_free(chip->gpio.vphy_id_gpio);
	if (gpio_is_valid(chip->gpio.vphy_switch2_gpio))
		gpio_free(chip->gpio.vphy_switch2_gpio);
	if (gpio_is_valid(chip->gpio.vphy_switch1_gpio))
		gpio_free(chip->gpio.vphy_switch1_gpio);
	if (gpio_is_valid(chip->gpio.switch_ctrl_gpio))
		gpio_free(chip->gpio.switch_ctrl_gpio);
	devm_kfree(&pdev->dev, chip->vphy_list);
child_init_err:
	devm_kfree(&pdev->dev, chip);
	platform_set_drvdata(pdev, NULL);

	return rc;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0))
static void oplus_virtual_vphy_remove(struct platform_device *pdev)
#else
static int oplus_virtual_vphy_remove(struct platform_device *pdev)
#endif
{
	struct oplus_virtual_vphy_ic *chip = platform_get_drvdata(pdev);

	if (chip == NULL) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0))
		return -ENODEV;
#else
		return;
#endif
	}

	if (chip->ic_dev->online)
		oplus_chg_vphy_exit(chip->ic_dev);
	devm_oplus_chg_ic_unregister(&pdev->dev, chip->ic_dev);
	if (gpio_is_valid(chip->gpio.vphy_id_gpio))
		gpio_free(chip->gpio.vphy_id_gpio);
	if (gpio_is_valid(chip->gpio.vphy_switch2_gpio))
		gpio_free(chip->gpio.vphy_switch2_gpio);
	if (gpio_is_valid(chip->gpio.vphy_switch1_gpio))
		gpio_free(chip->gpio.vphy_switch1_gpio);
	if (gpio_is_valid(chip->gpio.switch_ctrl_gpio))
		gpio_free(chip->gpio.switch_ctrl_gpio);
	devm_kfree(&pdev->dev, chip->vphy_list);
	devm_kfree(&pdev->dev, chip);
	platform_set_drvdata(pdev, NULL);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0))
	return 0;
#endif
}

static const struct of_device_id oplus_virtual_vphy_match[] = {
	{ .compatible = "oplus,virtual_vphy" },
	{},
};

static struct platform_driver oplus_virtual_vphy_driver = {
	.driver		= {
		.name = "oplus-virtual_vphy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(oplus_virtual_vphy_match),
	},
	.probe		= oplus_virtual_vphy_probe,
	.remove		= oplus_virtual_vphy_remove,
};

static __init int oplus_virtual_vphy_init(void)
{
	return platform_driver_register(&oplus_virtual_vphy_driver);
}

static __exit void oplus_virtual_vphy_exit(void)
{
	platform_driver_unregister(&oplus_virtual_vphy_driver);
}

oplus_chg_module_register(oplus_virtual_vphy);
