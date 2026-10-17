// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023-2023 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "[DPDM_SWITCH]([%s][%d]): " fmt, __func__, __LINE__

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
#include <linux/pinctrl/consumer.h>
#ifndef CONFIG_DISABLE_OPLUS_FUNCTION
#include <soc/oplus/system/boot_mode.h>
#include <soc/oplus/device_info.h>
#include <soc/oplus/system/oplus_project.h>
#endif
#include <oplus_chg_module.h>
#include <oplus_chg_ic.h>

/*Add for audio switch */
#include "oplus_chg_audio_switch.h"

#if IS_ENABLED(CONFIG_OPLUS_AUDIO_SWITCH_GLINK)
int register_chg_glink_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&chg_glink_notifier, nb);
}
EXPORT_SYMBOL_GPL(register_chg_glink_notifier);

int unregister_chg_glink_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&chg_glink_notifier, nb);
}
EXPORT_SYMBOL_GPL(unregister_chg_glink_notifier);
#endif

struct oplus_dpdm_switch_ic {
	struct device *dev;
	struct oplus_chg_ic_dev *ic_dev;

	int switch_ctrl_gpio;
	int dpdm_switch1_gpio;
	int dpdm_switch2_gpio;
	struct pinctrl_state *gpio_switch_ctrl_ap;
	struct pinctrl_state *gpio_switch_ctrl_vooc;
	struct pinctrl_state *gpio_switch_ctrl_ufcs;
	struct pinctrl_state *gpio_dpdm_switch_ap;
	struct pinctrl_state *gpio_dpdm_switch_vooc;
	struct pinctrl_state *gpio_dpdm_switch_ufcs;
	bool use_audio_switch;

	struct pinctrl *pinctrl;
	struct mutex pinctrl_lock;

	enum oplus_dpdm_switch_mode mode;
};

#define AUDIO_SWITCH_RETRY_MAX			3
#define OPLUS_AUDIO_SWITCH_READY_RETRY_MAX	50
#define OPLUS_AUDIO_SWITCH_READY_DELAY_TIME_MS	5

static int oplus_switch_to_ap(struct oplus_dpdm_switch_ic *chip)
{
	int rc;
	int retry;
	int status;

	mutex_lock(&chip->pinctrl_lock);
	if (!IS_ERR_OR_NULL(chip->pinctrl) && !IS_ERR_OR_NULL(chip->gpio_dpdm_switch_ap)) {
		rc = pinctrl_select_state(chip->pinctrl, chip->gpio_dpdm_switch_ap);
		if (rc < 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return rc;
		}
	}
	if (!chip->use_audio_switch) {
		rc = pinctrl_select_state(chip->pinctrl, chip->gpio_switch_ctrl_ap);
		if (rc < 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return rc;
		}
	} else {
		retry = AUDIO_SWITCH_RETRY_MAX;
		do {
			status = oplus_get_audio_switch_status();
			if (TYPEC_AUDIO_SWITCH_STATE_STANDBY & status) {
				break;
			}
			oplus_set_audio_switch_status(0);
			status = oplus_get_audio_switch_status();
			if ((status & TYPEC_AUDIO_SWITCH_STATE_MASK) | TYPEC_AUDIO_SWITCH_STATE_DPDM) {
			} else {
				break;
			}
		} while (--retry > 0);
		if (retry <= 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return -EFAULT;
		}
	}
	mutex_unlock(&chip->pinctrl_lock);

	return 0;
}

static int oplus_switch_to_vooc(struct oplus_dpdm_switch_ic *chip)
{
	int rc;
	int retry;
	int status;

	mutex_lock(&chip->pinctrl_lock);
	if (!IS_ERR_OR_NULL(chip->pinctrl) && !IS_ERR_OR_NULL(chip->gpio_dpdm_switch_vooc)) {
		rc = pinctrl_select_state(chip->pinctrl, chip->gpio_dpdm_switch_vooc);
		if (rc < 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return rc;
		}
	}
	if (!chip->use_audio_switch) {
		rc = pinctrl_select_state(chip->pinctrl, chip->gpio_switch_ctrl_vooc);
		if (rc < 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return rc;
		}
	} else {
		retry = AUDIO_SWITCH_RETRY_MAX;
		do {
			oplus_set_audio_switch_status(1);
			status = oplus_get_audio_switch_status();
			if (!(TYPEC_AUDIO_SWITCH_STATE_FAST_CHG & status) || (status < 0)) {
			} else {
				break;
			}
		} while (--retry > 0);
		if (retry <= 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return -EFAULT;
		}
	}
	mutex_unlock(&chip->pinctrl_lock);

	return 0;
}

static int oplus_switch_to_ufcs(struct oplus_dpdm_switch_ic *chip)
{
	int rc;
	int retry;
	int status;

	mutex_lock(&chip->pinctrl_lock);
	if (!IS_ERR_OR_NULL(chip->pinctrl) && !IS_ERR_OR_NULL(chip->gpio_dpdm_switch_ufcs)) {
		rc = pinctrl_select_state(chip->pinctrl, chip->gpio_dpdm_switch_ufcs);
		if (rc < 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return rc;
		}
	}
	if (!chip->use_audio_switch) {
		rc = pinctrl_select_state(chip->pinctrl, chip->gpio_switch_ctrl_ufcs);
		if (rc < 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return rc;
		}
	} else {
		retry = AUDIO_SWITCH_RETRY_MAX;
		do {
			oplus_set_audio_switch_status(1);
			status = oplus_get_audio_switch_status();
			if (!(TYPEC_AUDIO_SWITCH_STATE_FAST_CHG & status) || status < 0) {
			} else {
				break;
			}
		} while (--retry > 0);
		if (retry <= 0) {
			mutex_unlock(&chip->pinctrl_lock);
			return -EFAULT;
		}
	}
	mutex_unlock(&chip->pinctrl_lock);

	return 0;
}

static int oplus_dpdm_switch_init(struct oplus_chg_ic_dev *ic_dev)
{
	if (ic_dev == NULL) {
		return -ENODEV;
	}

	if (ic_dev->online)
		return 0;
	ic_dev->online = true;
	oplus_chg_ic_virq_trigger(ic_dev, OPLUS_IC_VIRQ_ONLINE);

	return 0;
}

static int oplus_dpdm_switch_exit(struct oplus_chg_ic_dev *ic_dev)
{
	if (ic_dev == NULL) {
		return -ENODEV;
	}
	if (!ic_dev->online)
		return 0;
	ic_dev->online = false;
	oplus_chg_ic_virq_trigger(ic_dev, OPLUS_IC_VIRQ_OFFLINE);

	return 0;
}

static int oplus_dpdm_switch_reg_dump(struct oplus_chg_ic_dev *ic_dev)
{
	return 0;
}

static int oplus_dpdm_switch_smt_test(struct oplus_chg_ic_dev *ic_dev, char buf[], int len)
{
	return 0;
}

static int oplus_dpdm_switch_set_switch_mode(struct oplus_chg_ic_dev *ic_dev,
	enum oplus_dpdm_switch_mode mode)
{
	struct oplus_dpdm_switch_ic *chip;
	int rc;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	chip = oplus_chg_ic_get_drvdata(ic_dev);

	switch (mode) {
	case DPDM_SWITCH_TO_AP:
		;
		rc = oplus_switch_to_ap(chip);
		break;
	case DPDM_SWITCH_TO_VOOC:
		;
		rc = oplus_switch_to_vooc(chip);
		break;
	case DPDM_SWITCH_TO_UFCS:
		;
		rc = oplus_switch_to_ufcs(chip);
		break;
	default:
		;
		return -EINVAL;
	}
	chip->mode = mode;

	return rc;
}

static int oplus_dpdm_switch_get_switch_mode(struct oplus_chg_ic_dev *ic_dev,
	enum oplus_dpdm_switch_mode *mode)
{
	struct oplus_dpdm_switch_ic *chip;

	if (ic_dev == NULL) {
		return -ENODEV;
	}

	chip = oplus_chg_ic_get_drvdata(ic_dev);
	*mode = chip->mode;

	return 0;
}

static void *oplus_dpdm_switch_get_func(struct oplus_chg_ic_dev *ic_dev, enum oplus_chg_ic_func func_id)
{
	void *func = NULL;

	if (!ic_dev->online && (func_id != OPLUS_IC_FUNC_INIT) &&
	    (func_id != OPLUS_IC_FUNC_EXIT)) {
		return NULL;
	}
	if (!oplus_chg_ic_func_is_support(ic_dev, func_id)) {
		return NULL;
	}

	switch (func_id) {
	case OPLUS_IC_FUNC_INIT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_INIT, oplus_dpdm_switch_init);
		break;
	case OPLUS_IC_FUNC_EXIT:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_EXIT, oplus_dpdm_switch_exit);
		break;
	case OPLUS_IC_FUNC_REG_DUMP:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_REG_DUMP, oplus_dpdm_switch_reg_dump);
		break;
	case OPLUS_IC_FUNC_SMT_TEST:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_SMT_TEST, oplus_dpdm_switch_smt_test);
		break;
	case OPLUS_IC_FUNC_SET_DPDM_SWITCH_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_SET_DPDM_SWITCH_MODE,
			oplus_dpdm_switch_set_switch_mode);
		break;
	case OPLUS_IC_FUNC_GET_DPDM_SWITCH_MODE:
		func = OPLUS_CHG_IC_FUNC_CHECK(OPLUS_IC_FUNC_GET_DPDM_SWITCH_MODE,
			oplus_dpdm_switch_get_switch_mode);
		break;
	default:
		;
		func = NULL;
		break;
	}

	return func;
}

struct oplus_chg_ic_virq oplus_dpdm_switch_virq_table[] = {
	{ .virq_id = OPLUS_IC_VIRQ_ERR },
	{ .virq_id = OPLUS_IC_VIRQ_ONLINE },
	{ .virq_id = OPLUS_IC_VIRQ_OFFLINE },
};

static int oplus_dpdm_switch_hw_init(struct oplus_dpdm_switch_ic *chip)
{
	struct device_node *node = chip->dev->of_node;
	int rc;

	chip->pinctrl = devm_pinctrl_get(chip->dev);

	chip->use_audio_switch = of_property_read_bool(node, "oplus,use_audio_switch");

	if (!chip->use_audio_switch) {
		if (IS_ERR_OR_NULL(chip->pinctrl)) {
			return -ENODEV;
		}
		chip->switch_ctrl_gpio =
			of_get_named_gpio(node, "oplus,switch_ctrl-gpio", 0);
		if (!gpio_is_valid(chip->switch_ctrl_gpio)) {
			return -ENODEV;
		}

		rc = gpio_request(chip->switch_ctrl_gpio, "switch_ctrl-gpio");
		if (rc != 0) {
			return rc;
		}

		chip->gpio_switch_ctrl_ap =
			pinctrl_lookup_state(chip->pinctrl, "switch_ctrl_ap");
		if (IS_ERR_OR_NULL(chip->gpio_switch_ctrl_ap)) {
			rc = -EINVAL;
			goto free_switch_ctrl_gpio;
		}
		chip->gpio_switch_ctrl_vooc =
			pinctrl_lookup_state(chip->pinctrl, "switch_ctrl_vooc");
		if (IS_ERR_OR_NULL(chip->gpio_switch_ctrl_vooc)) {
			rc = -EINVAL;
			goto free_switch_ctrl_gpio;
		}
		chip->gpio_switch_ctrl_ufcs =
			pinctrl_lookup_state(chip->pinctrl, "switch_ctrl_ufcs");
		if (IS_ERR_OR_NULL(chip->gpio_switch_ctrl_ufcs)) {
			rc = -EINVAL;
			goto free_switch_ctrl_gpio;
		}
		rc = pinctrl_select_state(chip->pinctrl,
			chip->gpio_switch_ctrl_ap);
		if (rc < 0) {
			goto free_switch_ctrl_gpio;
		}
	} else {
		audio_switch_init();
		if (IS_ERR_OR_NULL(chip->pinctrl)) {
			return 0;
		}
		/*TODO: need to check the default AUIDIO SWITCH state is DPDM and set to DPDM state.*/
	}

	chip->dpdm_switch1_gpio =
		of_get_named_gpio(node, "oplus,dpdm_switch1-gpio", 0);
	if (!gpio_is_valid(chip->dpdm_switch1_gpio)) {
	} else {
		rc = gpio_request(chip->dpdm_switch1_gpio, "dpdm_switch1-gpio");
		if (rc != 0) {
			goto free_switch_ctrl_gpio;
		}
	}
	chip->dpdm_switch2_gpio =
		of_get_named_gpio(node, "oplus,dpdm_switch2-gpio", 0);
	if (!gpio_is_valid(chip->dpdm_switch2_gpio)) {
	} else {
		rc = gpio_request(chip->dpdm_switch2_gpio,
				"dpdm_switch2-gpio");
		if (rc != 0) {
			goto free_dpdm_switch1_gpio;
		}
	}
	if (!gpio_is_valid(chip->dpdm_switch1_gpio) && !gpio_is_valid(chip->dpdm_switch2_gpio)) {
		return 0;
	}
	chip->gpio_dpdm_switch_ap =
		pinctrl_lookup_state(chip->pinctrl, "dpdm_switch_ap");
	if (IS_ERR_OR_NULL(chip->gpio_dpdm_switch_ap)) {
		rc = -EINVAL;
		goto free_dpdm_switch2_gpio;
	}
	chip->gpio_dpdm_switch_vooc =
		pinctrl_lookup_state(chip->pinctrl, "dpdm_switch_vooc");
	if (IS_ERR_OR_NULL(chip->gpio_dpdm_switch_vooc)) {
		rc = -EINVAL;
		goto free_dpdm_switch2_gpio;
	}
	chip->gpio_dpdm_switch_ufcs =
		pinctrl_lookup_state(chip->pinctrl, "dpdm_switch_ufcs");
	if (IS_ERR_OR_NULL(chip->gpio_dpdm_switch_ufcs)) {
		rc = -EINVAL;
		goto free_dpdm_switch2_gpio;
	}
	rc = pinctrl_select_state(chip->pinctrl,
		chip->gpio_dpdm_switch_ap);
	if (rc < 0) {
		goto free_dpdm_switch2_gpio;
	}

	return 0;

free_dpdm_switch2_gpio:
	if (gpio_is_valid(chip->dpdm_switch1_gpio))
		gpio_free(chip->dpdm_switch1_gpio);
free_dpdm_switch1_gpio:
	if (gpio_is_valid(chip->dpdm_switch1_gpio))
		gpio_free(chip->dpdm_switch1_gpio);
free_switch_ctrl_gpio:
	if (gpio_is_valid(chip->switch_ctrl_gpio))
		gpio_free(chip->switch_ctrl_gpio);

	return rc;
}

static int oplus_virtual_dpdm_switch_probe(struct platform_device *pdev)
{
	struct oplus_dpdm_switch_ic *chip;
	struct device_node *node = pdev->dev.of_node;
	struct oplus_chg_ic_cfg ic_cfg = { 0 };
	int rc;
	static int retry = 0;
	int status = -EFAULT;
	bool use_audio_switch = of_property_read_bool(node, "oplus,use_audio_switch");

	/* fix the issue of audio switch is not ready */
	if (use_audio_switch) {
		status = oplus_check_audio_switch_ready();
		if (status >= 0 || retry > OPLUS_AUDIO_SWITCH_READY_RETRY_MAX) {
		} else {
			retry++;
			msleep(OPLUS_AUDIO_SWITCH_READY_DELAY_TIME_MS);
			return -EPROBE_DEFER;
		}
	}

	chip = devm_kzalloc(&pdev->dev, sizeof(struct oplus_dpdm_switch_ic),
			    GFP_KERNEL);
	if (chip == NULL) {
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);

	mutex_init(&chip->pinctrl_lock);
	rc = oplus_dpdm_switch_hw_init(chip);
	if (rc < 0)
		goto hw_init_err;

	snprintf(ic_cfg.manu_name, OPLUS_CHG_IC_MANU_NAME_MAX - 1, "misc-dpdm_switch");
	snprintf(ic_cfg.fw_id, OPLUS_CHG_IC_FW_ID_MAX - 1, "0x00");
	ic_cfg.get_func = oplus_dpdm_switch_get_func;
	ic_cfg.virq_data = oplus_dpdm_switch_virq_table;
	ic_cfg.virq_num = ARRAY_SIZE(oplus_dpdm_switch_virq_table);
	ic_cfg.of_node = node;
	chip->ic_dev = devm_oplus_chg_ic_register(chip->dev, &ic_cfg);
	if (!chip->ic_dev) {
		rc = -ENODEV;
		goto reg_ic_err;
	}
	oplus_dpdm_switch_init(chip->ic_dev);

	return 0;

reg_ic_err:
	if (gpio_is_valid(chip->switch_ctrl_gpio))
		gpio_free(chip->switch_ctrl_gpio);
hw_init_err:
	devm_kfree(&pdev->dev, chip);
	platform_set_drvdata(pdev, NULL);

	return rc;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0))
static void oplus_virtual_dpdm_switch_remove(struct platform_device *pdev)
#else
static int oplus_virtual_dpdm_switch_remove(struct platform_device *pdev)
#endif
{
	struct oplus_dpdm_switch_ic *chip = platform_get_drvdata(pdev);

	if (chip == NULL) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0))
		return -ENODEV;
#else
		return;
#endif
	}

	if (chip->ic_dev->online)
		oplus_dpdm_switch_exit(chip->ic_dev);

	devm_oplus_chg_ic_unregister(&pdev->dev, chip->ic_dev);
	if (gpio_is_valid(chip->switch_ctrl_gpio))
		gpio_free(chip->switch_ctrl_gpio);
	devm_kfree(&pdev->dev, chip);
	platform_set_drvdata(pdev, NULL);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0))
	return 0;
#endif
}

static const struct of_device_id oplus_virtual_dpdm_switch_match[] = {
	{ .compatible = "oplus,virtual_dpdm_switch" },
	{},
};

static struct platform_driver oplus_virtual_dpdm_switch_driver = {
	.driver		= {
		.name = "oplus-virtual_dpdm_switch",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(oplus_virtual_dpdm_switch_match),
	},
	.probe		= oplus_virtual_dpdm_switch_probe,
	.remove		= oplus_virtual_dpdm_switch_remove,
};

static __init int oplus_virtual_dpdm_switch_init(void)
{
#if __and(IS_BUILTIN(CONFIG_OPLUS_CHG), IS_BUILTIN(CONFIG_OPLUS_CHG_V2))
	struct device_node *node;

	node = of_find_node_by_path("/soc/oplus_chg_core");
	if (node == NULL)
		return 0;
	if (!of_property_read_bool(node, "oplus,chg_framework_v2"))
		return 0;
#endif /* CONFIG_OPLUS_CHG_V2 */
	return platform_driver_register(&oplus_virtual_dpdm_switch_driver);
}

static __exit void oplus_virtual_dpdm_switch_exit(void)
{
#if (IS_ENABLED(CONFIG_OPLUS_CHG) && IS_ENABLED(CONFIG_OPLUS_CHG_V2))
	struct device_node *node;

	node = of_find_node_by_path("/soc/oplus_chg_core");
	if (node == NULL)
		return;
	if (!of_property_read_bool(node, "oplus,chg_framework_v2"))
		return;
#endif /* CONFIG_OPLUS_CHG_V2 */
	platform_driver_unregister(&oplus_virtual_dpdm_switch_driver);
}

oplus_chg_module_register(oplus_virtual_dpdm_switch);
