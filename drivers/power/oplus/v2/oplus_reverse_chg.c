/***********************************************************
** Copyright (C), 2025-2025 Oplus. All rights reserved.
** File: oplus_reverse_chg.c
** Description: cpreverse ic
** Date: 2025-11-14
** -----------Revision History: -------------------------------
** <author>        <data>    <version >       <desc>
****************************************************************/

#define pr_fmt(fmt) "[REVERSE]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/gfp.h>
#include <linux/kthread.h>
#include <linux/sched/clock.h>
#include <linux/proc_fs.h>

#include <oplus_chg.h>
#include <oplus_chg_module.h>
#include <oplus_chg_monitor.h>
#include <oplus_chg_ic.h>
#include <oplus_mms_gauge.h>
#include <oplus_mms_wired.h>
#include <oplus_chg_comm.h>
#include <oplus_chg_vooc.h>
#include <oplus_chg_voter.h>
#include <oplus_chg_cpa.h>
#include <oplus_chg_ufcs.h>
#include <oplus_chg_pps.h>
#include <oplus_reverse_chg.h>

#define DEC_REVERSE_UISOC_LINIT_COUNT	4
#define DEC_REVERSE_TEMP_LINIT_COUNT	4
#define DEC_REVERSE_VBUS_LINIT_COUNT	5
#define DEC_REVERSE_VBAT_LINIT_COUNT	5
#define DEC_REVERSE_PDO_LINIT_COUNT	6
#define DEC_LAST_LINIT_COUNT		2
#define MAX_NORMAL_REVERSE_COUNT	5
#define MAX_HARDRESET_LIMIT_COUNT	6
#define HIGH_VBUS_START			6000
#define MIN_HIGH_REVERSE_POWER		15


struct reverse_limit_data {
	int reverse_uisoc_limit[DEC_REVERSE_UISOC_LINIT_COUNT];
	int reverse_temp_limit[DEC_REVERSE_TEMP_LINIT_COUNT];
	int reverse_vbus_limit[DEC_REVERSE_VBUS_LINIT_COUNT];
	int reverse_vbat_limit[DEC_REVERSE_VBAT_LINIT_COUNT];
	int last_uisoc_limit[DEC_LAST_LINIT_COUNT];
	int last_current[DEC_LAST_LINIT_COUNT];
	int reverse_led_on_limit[DEC_LAST_LINIT_COUNT];
	int reverse_authenticate_limit[DEC_LAST_LINIT_COUNT];
	int reverse_over_ibus_limit[DEC_LAST_LINIT_COUNT];
	int normal_reverse_current[MAX_NORMAL_REVERSE_COUNT];
	int normal_reverse_hw_ocp[MAX_NORMAL_REVERSE_COUNT];
};

struct oplus_reverse_pdo_limit_t {
	int reverse_pdo_voltage;
	int reverse_pdo_current;
	u32 reverse_pdo;
};

struct oplus_reverse_temp_pdo_limit {
	struct oplus_reverse_pdo_limit_t comm_pdo_limit;
	struct oplus_reverse_pdo_limit_t oplus_pdo_limit;
};

struct oplus_reverse_uisoc_pdo_limit {
	struct oplus_reverse_temp_pdo_limit reverse_temp_pdo_limit[DEC_REVERSE_TEMP_LINIT_COUNT + 1];
};

struct oplus_chg_reverse {
	struct device *dev;
	struct proc_dir_entry *reverse_entry;
	struct oplus_mms *reverse_topic;
	struct oplus_mms *comm_topic;
	struct oplus_mms *wired_topic;
	struct oplus_mms *cpa_topic;
	struct mms_subscribe *comm_subs;
	struct mms_subscribe *wired_subs;
	struct mms_subscribe *reverse_subs;
	struct mms_subscribe *gauge_subs;
	struct oplus_chg_ic_dev *cp_ic;
	struct oplus_chg_ic_dev *buck_ic;
	struct oplus_chg_ic_dev *reverse_ic;
	int shell_temp;

	struct oplus_chg_strategy *temperature_strategy;
	struct delayed_work reverse_monitor_work;
	struct delayed_work usbtemp_high_work;
	struct delayed_work reverse_pdo_update_work;
	struct delayed_work reverse_chg_init_work;
	struct delayed_work reverse_chg_type_work;
	struct delayed_work reverse_error_flag_work;
	struct delayed_work normal_reverse_set_hw_ocp_work;
	struct delayed_work reverse_vbus_retention_check_work;
	struct delayed_work reverse_chg_test_mode_work;
	struct delayed_work reverse_clear_keep_status_work;
	struct delayed_work reverse_hard_reset_work;
	struct work_struct reverse_online_work;
	struct work_struct reverse_vbus_check_work;

	struct mutex set_pdo_lock;
	bool reverse_chg_switch;
	bool reverse_chg_testing;
	int reverse_chg_test_mode;
	int test_high_pwr_vbus_mv;
	int test_result;
	bool wired_online;
	int vbus_mv;
	int vbus_level;
	int vbus_limit_count;
	int vbat_level;
	int vbat_limit_count;
	int hardreset_limit_count;
	int hw_detect;
	int reverse_status;
	int reverse_enable;
	int high_reverse_enable;
	int high_reverse_count;
	int enable_err_count;
	pd_msg_data pdo[PPS_PDO_MAX];

	int reverse_temp_cur_range;
	int reverse_uisoc_cur_range;
	bool high_reverse_charging;
	int reverse_max_curr;
	bool start_check;
	int ibus_ma;
	int reverse_strategy_change_count;
	int reverse_ibus_lower_oplus;
	struct oplus_chg_strategy *strategy;
	struct votable *reverse_pdo_votable;
	struct votable *cap_pdo0_votable;
	struct votable *high_reverse_disable_votable;
	struct oplus_mms *gauge_topic;
	struct oplus_mms *sub_gauge_topic;
	int ibus_over;
	int ibus_lower;
	bool ibus_ov;
	int thired_oplus_svid_limit;
	bool use_high_reverse_awake;
	bool kthread_reverse_enable;
	bool reverse_vote;
	bool use_cp_reverse;
	bool authenticate;
	bool hmac;
	bool sink_svid_support;
	bool is_support_high_reverse;
	int ui_soc;
	int led_on;
	int vbat_min_mv;
	int vbat_mv;
	int ibat_ma;
	int batt_temp;
	int batt_soc;
	int batt_realy_temp;
	int hard_reset_count;
	int drop_down_count;
	int reset_watchdog_time;
	int pre_pdo_voltage;
	int pre_pdo_current;
	int high_reverse_err_flag;
	int normal_reverse_count;
	int req_volt;
	int req_current;
	int reverse_chg_type;
	int oplus_svid;
	u32 target_pdo;
	u32 target_cap_pdo0;
	u32 pre_target_cap_pdo0;
	u32 led_on_pdo_limit;
	u32 authenticate_pdo_limit;
	u32 over_ibus_pdo_limit;
	u32 reverse_chg_svid;
	u32 last_high_reverse_pdo;
	int reverse_vbus_pdo_limit_num;
	u32 max_reverse_power;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	struct wake_lock reverse_wake_lock;
#else
	struct wakeup_source *reverse_ws;
#endif
	struct task_struct *reverse_watchdog;
	wait_queue_head_t reverse_watchdog_wq;
	struct reverse_limit_data reverse_limit;
	struct oplus_reverse_uisoc_pdo_limit
		reverse_soc_pdo_limit[DEC_REVERSE_PDO_LINIT_COUNT];
	struct oplus_reverse_pdo_limit_t reverse_vbus_pdo_limit[DEC_REVERSE_PDO_LINIT_COUNT];
	struct oplus_reverse_pdo_limit_t reverse_hardreset_pdo_limit[DEC_REVERSE_PDO_LINIT_COUNT];
};
static struct oplus_chg_reverse *g_rvs_chg_chip = NULL;

enum oplus_last_reason {
	LAST_LEVEL_1,
	LAST_LEVEL_2
};

enum oplus_uisoc_reason {
	UISOC_LEVEL_MIN, /* 20-30% */
	UISOC_LEVEL_MID, /* 30-50% */
	UISOC_LEVEL_NORMAL, /*50- 70% */
	UISOC_LEVEL_MID_HIGH, /*70- 80% */
	UISOC_LEVEL_HIGH, /*80- 100% */
	UISOC_LEVEL_INVALID
};

enum oplus_temp_reason {
	TEMP_LEVLE_RANGE_COLD = 0,
	TEMP_LEVLE_RANGE_COOL, /* 0 ~ 16 */
	TEMP_LEVLE_RANGE_NORMAL, /* 16 ~ 45 */
	TEMP_LEVLE_RANGE_WARM, /* 45 ~ 52 */
	TEMP_LEVLE_RANGE_HIGH,
};

enum oplus_reverse_pdo_reason {
	REVERSE_PDO_FIRST_LEVEL, /* 5V1.5A */
	REVERSE_PDO_SECOND_LEVEL, /* 9V1A */
	REVERSE_PDO_THIRD_LEVEL, /* 9V1.5A */
	REVERSE_PDO_FOURTH_LEVEL, /* 9V2A */
	REVERSE_PDO_FIFTH_LEVEL, /* 9V2.5A */
	REVERSE_PDO_SIXTH_LEVEL, /* 9V3A */
	REVERSE_PDO_MAX,
};

static const char *disable_vote_level_names[] = {
	"USER_VOTER",
	"BATT_TEMP_VOTER",
	"BATT_SOC_VOTER",
	"CURR_ERR_VOTER",
	"CURR_LIMIT_VOTER",
	"USB_VOTER",
	"HW_ERR_VOTER",
	"BATT_VOL_VOTER",
	"AUTH_VOTER",
	"SVID_VOTER",
	"NO_DATA_VOTER"
};

static void oplus_reverse_set_awake(struct oplus_chg_reverse *chip, bool awake);

#define CANNOT_OPEN_HIGH_PWR_PATH_VOL_MV 2000
#define LOWER_IBUS 500
#define NORMAL_VBUS 5000
#define PD_VBUS 9000
#define NORMAL_IBUS 1000
#define REVERSE_IBUS 1500
#define WAIT_HIGH_REVERSE_COLSE 1000

__maybe_unused static bool
is_gauge_topic_available(struct oplus_chg_reverse *chip)
{
	if (!chip->gauge_topic)
		chip->gauge_topic = oplus_mms_get_by_name("gauge");
	return !!chip->gauge_topic;
}


__maybe_unused static bool
is_comm_topic_available(struct oplus_chg_reverse *chip)
{
	if (!chip->comm_topic)
		chip->comm_topic = oplus_mms_get_by_name("common");
	return !!chip->comm_topic;
}
#define PDO_LENGTH_LIMIT 2
static int oplus_reverse_uisoc_pdo_limit_parse_dt(struct oplus_chg_reverse *chip)
{
	int i = 0;
	int j = 0;
	int rc = 0;
	int num_elems = 0;
	uint32_t comm_data[2];
	uint32_t data[2 * PDO_LENGTH_LIMIT];
	u32 pdo;
	u32 comm_pdo;
	u32 oplus_pdo;
	struct device_node *node = chip->dev->of_node;
	struct device_node *uisoc_list_node, *temp_node;
	const char *temp_level_names[] = {
		"temp_level_range_cold",
		"temp_level_range_cool",
		"temp_level_range_normal",
		"temp_level_range_warm",
		"temp_level_range_high"
	};
	const char *soc_node_names[] = {
		"reverse_uisoc_range_min_pdo_limit_list",
		"reverse_uisoc_range_mid_pdo_limit_list",
		"reverse_uisoc_range_normal_pdo_limit_list",
		"reverse_uisoc_range_mid_high_pdo_limit_list",
		"reverse_uisoc_range_high_pdo_limit_list"
	};

	uisoc_list_node = of_get_child_by_name(node,
		"oplus,reverse_uisoc_pdo_limit_list");
	if (!uisoc_list_node) {
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(soc_node_names); i++) {
		temp_node = of_get_child_by_name(uisoc_list_node, soc_node_names[i]);
		if (!temp_node) {
			of_node_put(temp_node);
			break;
		}

		for (j = 0; j < ARRAY_SIZE(temp_level_names); j++) {
			num_elems = of_property_count_elems_of_size(temp_node,
				temp_level_names[j], sizeof(u32));
			if (num_elems < 0) {
				break;
			}
			if (num_elems == PDO_LENGTH_LIMIT) {
				rc = of_property_read_u32_array(temp_node,
					temp_level_names[j], comm_data, 2);
				if (rc < 0) {
					of_node_put(temp_node);
					goto out;
				}

				/* oplus use comm */
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].comm_pdo_limit.
					reverse_pdo_voltage = comm_data[0];
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].comm_pdo_limit.
					reverse_pdo_current = comm_data[1];
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].oplus_pdo_limit.
					reverse_pdo_voltage = comm_data[0];
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].oplus_pdo_limit.
					reverse_pdo_current = comm_data[1];

				pdo = (comm_data[0] << 16) | (comm_data[1] & 0xFFFF);
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].
					comm_pdo_limit.reverse_pdo = pdo;
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].
					oplus_pdo_limit.reverse_pdo = pdo;
			} else if (num_elems == 2 * PDO_LENGTH_LIMIT) {
				rc = of_property_read_u32_array(temp_node, temp_level_names[j], data, 4);
				if (rc < 0) {
					of_node_put(temp_node);
					goto out;
				}

				/* comm */
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].comm_pdo_limit.
					reverse_pdo_voltage = data[0];
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].comm_pdo_limit.
					reverse_pdo_current = data[1];

				/* oplus  */
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].oplus_pdo_limit.
					reverse_pdo_voltage = data[2];
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].oplus_pdo_limit.
					reverse_pdo_current = data[3];

				comm_pdo = (data[0] << 16) | (data[1] & 0xFFFF);
				oplus_pdo = (data[2] << 16) | (data[3] & 0xFFFF);
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].
					comm_pdo_limit.reverse_pdo = comm_pdo;
				chip->reverse_soc_pdo_limit[i].reverse_temp_pdo_limit[j].
					oplus_pdo_limit.reverse_pdo = oplus_pdo;
			} else {
				rc = -EINVAL;
				goto out;
			}
		}
		of_node_put(temp_node);
	}

	rc = 0;
out:
	of_node_put(uisoc_list_node);
	return rc;
}

static int oplus_reverse_vbus_pdo_limit_parse_dt(struct oplus_chg_reverse *chip)
{
	int i;
	int num;
	int rc = 0;
	uint32_t data;
	struct device_node *node = chip->dev->of_node;

	num = of_property_count_elems_of_size(node, "oplus,reverse_vbus_pdo_limit_list", sizeof(u32));

	chip->is_support_high_reverse = false;
	if (num < 2) {
		return num;
	} else if ((num >> 1) > DEC_REVERSE_PDO_LINIT_COUNT) {
		num = DEC_REVERSE_PDO_LINIT_COUNT;
		return num;
	} else {
		num /= 2;
		chip->reverse_vbus_pdo_limit_num = num;
	}

	for (i = 0; i < num; i++) {
		rc = of_property_read_u32_index(node, "oplus,reverse_vbus_pdo_limit_list", i * 2, &data);
		if (rc < 0) {
			continue;
		} else {
			if (data > 10000) {
				continue;
			} else {
				chip->reverse_vbus_pdo_limit[i].reverse_pdo_voltage = data;
				if (chip->reverse_vbus_pdo_limit[i].reverse_pdo_voltage >= PD_VBUS)
					chip->is_support_high_reverse = true;
			}
		}
		rc = of_property_read_u32_index(node, "oplus,reverse_vbus_pdo_limit_list", i * 2 + 1, &data);
		if (rc < 0) {
			continue;
		} else {
			chip->reverse_vbus_pdo_limit[i].reverse_pdo_current = data;
		}
		chip->reverse_vbus_pdo_limit[i].reverse_pdo =
		chip->reverse_vbus_pdo_limit[i].reverse_pdo_voltage << 16 | chip->reverse_vbus_pdo_limit[i].reverse_pdo_current;
	}

	return 0;
}

static int oplus_normal_reverse_hw_ocp_parse_dt(struct oplus_chg_reverse *chip)
{
	int i;
	int num;
	int rc = 0;
	uint32_t data;
	struct device_node *node = chip->dev->of_node;

	num = of_property_count_elems_of_size(node,
		"oplus,normal_reverse_hw_ocp", sizeof(u32));
	if (num < 2) {
		return num;
	} else if ((num >> 1) > MAX_NORMAL_REVERSE_COUNT) {
		num = MAX_NORMAL_REVERSE_COUNT;
		return num;
	} else {
		num /= 2;
		chip->normal_reverse_count = num;
	}
	for (i = 0; i < num; i++) {
		rc = of_property_read_u32_index(node,
			"oplus,normal_reverse_hw_ocp", i * 2, &data);
		if (rc < 0) {
			continue;
		} else {
			if (data > 10000) {
				continue;
			} else {
				chip->reverse_limit.normal_reverse_current[i] = data;
			}
		}
		rc = of_property_read_u32_index(node,
			"oplus,normal_reverse_hw_ocp", i * 2 + 1, &data);
		if (rc < 0) {
			continue;
		} else {
			chip->reverse_limit.normal_reverse_hw_ocp[i] = data;
		}
	}
	return 0;
}

static int oplus_reverse_hardreset_limit_parse_dt(struct oplus_chg_reverse *chip)
{
	int i;
	int num;
	int rc = 0;
	uint32_t data;
	struct device_node *node = chip->dev->of_node;

	num = of_property_count_elems_of_size(node,
		"oplus,reverse_hardreset_limit", sizeof(u32));
	if (num < 2) {
		return num;
	} else if ((num >> 1) > MAX_HARDRESET_LIMIT_COUNT) {
		num = MAX_HARDRESET_LIMIT_COUNT;
		return num;
	} else {
		num /= 2;
		chip->hardreset_limit_count = num;
	}
	for (i = 0; i < num; i++) {
		rc = of_property_read_u32_index(node,
			"oplus,reverse_hardreset_limit", i * 2, &data);
		if (rc < 0) {
			continue;
		} else {
			if (data > 10000) {
				continue;
			} else {
				chip->reverse_hardreset_pdo_limit[i].reverse_pdo_voltage = data;
			}
		}
		rc = of_property_read_u32_index(node,
			"oplus,reverse_hardreset_limit", i * 2 + 1, &data);
		if (rc < 0) {
			continue;
		} else {
			chip->reverse_hardreset_pdo_limit[i].reverse_pdo_current = data;
		}
		chip->reverse_hardreset_pdo_limit[i].reverse_pdo =
		chip->reverse_hardreset_pdo_limit[i].reverse_pdo_voltage << 16 | chip->reverse_hardreset_pdo_limit[i].reverse_pdo_current;
	}
	return 0;
}

static int oplus_reverse_parse_dt(struct oplus_chg_reverse *chip)
{
	struct device_node *node = oplus_get_node_by_type(chip->dev->of_node);
	int rc;
	int i;

	rc = of_property_count_elems_of_size(node, "oplus_spec,reverse-uisoc-limit", sizeof(u32));
	if (rc > 0 && rc <= DEC_REVERSE_UISOC_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,reverse-uisoc-limit", (u32 *)chip->reverse_limit.reverse_uisoc_limit, rc);
	}
	for (i = 0; i < DEC_REVERSE_UISOC_LINIT_COUNT; i++) {
	}
	rc = of_property_count_elems_of_size(node, "oplus_spec,reverse-temp-limit", sizeof(u32));
	if (rc > 0 && rc <= DEC_REVERSE_TEMP_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,reverse-temp-limit", (u32 *)chip->reverse_limit.reverse_temp_limit, rc);
	}
	for (i = 0; i < DEC_REVERSE_TEMP_LINIT_COUNT; i++) {
	}
	rc = of_property_count_elems_of_size(node, "oplus_spec,reverse-vbus-limit", sizeof(u32));
	chip->vbus_limit_count = rc;
	if (rc > 0 && rc <= DEC_REVERSE_VBUS_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,reverse-vbus-limit", (u32 *)chip->reverse_limit.reverse_vbus_limit, rc);
	}
	for (i = 0; i < chip->vbus_limit_count; i++) {
	}
	rc = of_property_count_elems_of_size(node, "oplus_spec,reverse-vbat-limit", sizeof(u32));
	chip->vbat_limit_count = rc;
	if (rc > 0 && rc <= DEC_REVERSE_VBAT_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,reverse-vbat-limit", (u32 *)chip->reverse_limit.reverse_vbat_limit, rc);
	}
	for (i = 0; i < chip->vbat_limit_count; i++) {
	}
	rc = of_property_count_elems_of_size(node, "oplus_spec,last-uisoc-limit", sizeof(u32));
	if (rc > 0 && rc <= DEC_LAST_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,last-uisoc-limit", (u32 *)chip->reverse_limit.last_uisoc_limit, rc);
	}
	for (i = 0; i < DEC_LAST_LINIT_COUNT; i++) {
	}
	rc = of_property_count_elems_of_size(node, "oplus_spec,last-current", sizeof(u32));
	if (rc > 0 && rc <= DEC_LAST_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,last-current", (u32 *)chip->reverse_limit.last_current, rc);
	}
	for (i = 0; i < DEC_LAST_LINIT_COUNT; i++) {
	}
	rc = of_property_count_elems_of_size(node, "oplus_spec,reverse-led-on-limit", sizeof(u32));
	if (rc > 0 && rc <= DEC_LAST_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,reverse-led-on-limit", (u32 *)chip->reverse_limit.reverse_led_on_limit, rc);
		for (i = 0; i < DEC_LAST_LINIT_COUNT; i++) {
		}
		chip->led_on_pdo_limit =
		chip->reverse_limit.reverse_led_on_limit[LAST_LEVEL_1] << 16 | chip->reverse_limit.reverse_led_on_limit[LAST_LEVEL_2];
	} else {
		chip->led_on_pdo_limit = 0;
	}
	rc = of_property_count_elems_of_size(node,
		"oplus_spec,reverse-authenticate-limit", sizeof(u32));
	if (rc > 0 && rc <= DEC_LAST_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,reverse-authenticate-limit",
			(u32 *)chip->reverse_limit.reverse_authenticate_limit, rc);
	}
	for (i = 0; i < DEC_LAST_LINIT_COUNT; i++) {
	}
	chip->authenticate_pdo_limit =
		(chip->reverse_limit.reverse_authenticate_limit[LAST_LEVEL_1] << 16) |
		(chip->reverse_limit.reverse_authenticate_limit[LAST_LEVEL_2]);

	rc = of_property_count_elems_of_size(node,
		"oplus_spec,reverse-over-ibus-limit", sizeof(u32));
	if (rc > 0 && rc <= DEC_LAST_LINIT_COUNT) {
		rc = of_property_read_u32_array(node, "oplus_spec,reverse-over-ibus-limit",
			(u32 *)chip->reverse_limit.reverse_over_ibus_limit, rc);
	}
	for (i = 0; i < DEC_LAST_LINIT_COUNT; i++) {
	}
	chip->over_ibus_pdo_limit =
		(chip->reverse_limit.reverse_over_ibus_limit[LAST_LEVEL_1] << 16) |
		(chip->reverse_limit.reverse_over_ibus_limit[LAST_LEVEL_2]);

	rc = of_property_read_u32(node, "oplus,reverse_ibus_lower_oplus", &chip->reverse_ibus_lower_oplus);
	if (rc) {
		chip->reverse_ibus_lower_oplus = 100;
	}
	rc = of_property_read_u32(node, "oplus,reset_watchdog_time", &chip->reset_watchdog_time);
	if (rc) {
		chip->reset_watchdog_time = 1000;
	}
	rc = of_property_read_u32(node, "oplus,max_reverse_power", &chip->max_reverse_power);
	if (rc) {
		chip->max_reverse_power = 27;
	}
	chip->use_high_reverse_awake = of_property_read_bool(node, "oplus,use_high_reverse_awake");
	chip->use_cp_reverse = of_property_read_bool(node, "oplus,use_cp_reverse");
	chip->sink_svid_support = of_property_read_bool(node, "oplus,sink_svid_support");
	oplus_reverse_uisoc_pdo_limit_parse_dt(chip);
	oplus_reverse_vbus_pdo_limit_parse_dt(chip);
	oplus_reverse_hardreset_limit_parse_dt(chip);
	rc = of_property_read_u32(node, "oplus,thired_oplus_svid_limit", &chip->thired_oplus_svid_limit);
	if (rc) {
		chip->thired_oplus_svid_limit =
			chip->reverse_soc_pdo_limit[UISOC_LEVEL_MIN].
			reverse_temp_pdo_limit[TEMP_LEVLE_RANGE_COLD].
			comm_pdo_limit.reverse_pdo_current;
	}
	oplus_normal_reverse_hw_ocp_parse_dt(chip);
	return 0;
}

#define ENABLE_ERROR_COUNT_LEVEL 3
#define CHECK_BOOST_VOL_DELAY 1000
static int oplus_reverse_get_boost_vol(
	struct oplus_chg_reverse *chip, int *vout);
static void oplus_reverse_error_flag_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, reverse_error_flag_work);
	static int pre_high_reverse_err_flag;
	static int pre_hard_reset_count;
	struct mms_msg *msg;
	int cp_vbus_mv;
	int rc;
	int i;

	if (chip->max_reverse_power <= MIN_HIGH_REVERSE_POWER)
		return;

	if (get_client_vote(chip->high_reverse_disable_votable, USER_VOTER)) {
		chip->high_reverse_err_flag = HPR_USER_VOTER_DISABLE;
	} else if (get_client_vote(
		chip->high_reverse_disable_votable, BATT_TEMP_VOTER)) {
		chip->high_reverse_err_flag = HPR_BATT_TEMP_VOTER_DISABLE;
	} else if (get_client_vote(
		chip->high_reverse_disable_votable, BATT_SOC_VOTER)) {
		chip->high_reverse_err_flag = HPR_BATT_SOC_VOTER_DISABLE;
	} else if (get_client_vote(
		chip->high_reverse_disable_votable, CURR_ERR_VOTER)) {
		chip->high_reverse_err_flag = HPR_CURR_ERR_VOTER_DISABLE;
	} else if (get_client_vote(
		chip->high_reverse_disable_votable, AUTH_VOTER)) {
		chip->high_reverse_err_flag = HPR_AUTH_VOTER_DISABLE;
	} else if (get_client_vote(
		chip->high_reverse_disable_votable, SVID_VOTER)) {
		chip->high_reverse_err_flag = HPR_SVID_VOTER_DISABLE;
	} else if (get_client_vote(
		chip->high_reverse_disable_votable, USB_VOTER)) {
		chip->high_reverse_err_flag = HPR_USB_VOTER_DISABLE;
	} else if (get_client_vote(
		chip->high_reverse_disable_votable, HW_ERR_VOTER) ||
		(chip->hard_reset_count > 0 &&
		chip->hard_reset_count != pre_hard_reset_count)) {
		pre_hard_reset_count = chip->hard_reset_count;
		chip->high_reverse_err_flag = HPR_HW_ERR_VOTER_DISABLE;
	} else if (chip->pre_pdo_voltage > NORMAL_VBUS &&
		chip->req_volt > NORMAL_VBUS) {
		rc = oplus_reverse_get_boost_vol(chip, &cp_vbus_mv);
		if (rc < 0) {
			return;
		}
		if (cp_vbus_mv <= HIGH_VBUS_START) {
			for (i = 0; i <= ENABLE_ERROR_COUNT_LEVEL; i++) {
				msleep(CHECK_BOOST_VOL_DELAY);
				oplus_reverse_get_boost_vol(chip, &cp_vbus_mv);
				if (!chip->reverse_enable)
					return;
				if (cp_vbus_mv <= HIGH_VBUS_START)
					continue;
				else
					break;
			}
		}
		if (cp_vbus_mv <= HIGH_VBUS_START)
			chip->high_reverse_err_flag = HPR_DEVICE_ERROR_DISABLE;
		else
			chip->high_reverse_err_flag = HPR_NO_VOTER_DISABLE;
	} else {
		chip->high_reverse_err_flag = HPR_NO_VOTER_DISABLE;
	}

	if (chip->high_reverse_err_flag > 0 && chip->high_reverse_err_flag !=
		pre_high_reverse_err_flag) {
		msg = oplus_mms_alloc_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
						REVERSE_ITEM_HIGH_REVERSE_ERR);
		if (msg == NULL) {
			return;
		}
		rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
		if (rc < 0) {
			kfree(msg);
		}
	}
	pre_high_reverse_err_flag = chip->high_reverse_err_flag;

	return;
}

static int oplus_reverse_set_source_pdo
	(struct oplus_chg_reverse *chip, int pdo0_volt, int pdo0_curr, int pdo1_volt, int pdo1_curr)
{
	int rc;

	rc = oplus_chg_ic_func(chip->reverse_ic,
			OPLUS_IC_FUNC_SET_REVERSE_SRC_PDO,
			pdo0_volt, pdo0_curr, pdo1_volt, pdo1_curr);
	if (rc < 0) {
		return -1;
	}
	return 0;
}

static int oplus_set_reverse_chg_type_ufcs(struct oplus_chg_reverse *chip, bool enable)
{
	return 0;
}

static int oplus_set_reverse_chg_type_pps(struct oplus_chg_reverse *chip, bool enable)
{
	return 0;
}

static int oplus_set_reverse_chg_type_pd(struct oplus_chg_reverse *chip, bool enable)
{
	int rc = -1;
	if (!chip->reverse_ic) {
		return -1;
	}

	if (enable) {
		vote(chip->high_reverse_disable_votable, USER_VOTER, false, 0, false);
		oplus_reverse_set_source_pdo(chip, NORMAL_VBUS, NORMAL_IBUS, PD_VBUS, NORMAL_IBUS);
	} else {
		vote(chip->high_reverse_disable_votable, USER_VOTER, true, 1, false);
		oplus_reverse_set_source_pdo(chip, NORMAL_VBUS, NORMAL_IBUS, NORMAL_VBUS, NORMAL_IBUS);
	}
	return rc;
}
static bool oplus_check_reverse_chg_is_ufcs_type(struct oplus_chg_reverse *chip)
{
	return false;
}

static bool oplus_check_reverse_chg_is_pps_type(struct oplus_chg_reverse *chip)
{
	int type = 0;
	int rc;

	if (chip->reverse_ic) {
		rc = oplus_chg_ic_func(chip->reverse_ic,
			OPLUS_IC_FUNC_GET_REVERSE_CHG_TYPE, &type);
	}
	if (type == BIT(1))
		return true;
	return false;
}
static bool oplus_check_reverse_chg_is_pd_type(struct oplus_chg_reverse *chip)
{
	int type = 0;
	int rc;

	if (chip->reverse_ic) {
		rc = oplus_chg_ic_func(chip->reverse_ic,
			OPLUS_IC_FUNC_GET_REVERSE_CHG_TYPE, &type);
	}
	if (type == BIT(0))
		return true;

	return false;
}
static void oplus_reverse_chg_type_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, reverse_chg_type_work);
	struct mms_msg *msg;
	static int pre_reverse_chg_type = 0;
	int rc;

	if (oplus_check_reverse_chg_is_ufcs_type(chip))
		chip->reverse_chg_type = REVERSE_CHG_TYPE_UFCS;
	else if (oplus_check_reverse_chg_is_pps_type(chip))
		chip->reverse_chg_type = REVERSE_CHG_TYPE_PPS;
	else if (oplus_check_reverse_chg_is_pd_type(chip))
		chip->reverse_chg_type = REVERSE_CHG_TYPE_PD;
	else if (chip->reverse_enable)
		chip->reverse_chg_type = REVERSE_CHG_TYPE_NORMAL;
	else
		chip->reverse_chg_type = REVERSE_CHG_TYPE_UNKNOWN;

	if (chip->reverse_chg_type != pre_reverse_chg_type) {
		msg = oplus_mms_alloc_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
						REVERSE_ITEM_REVERSE_CHG_TYPE);
		if (msg == NULL) {
			return;
		}
		rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
		if (rc < 0) {
			kfree(msg);
		}
		pre_reverse_chg_type = chip->reverse_chg_type;
	}
}

#define HARD_RESET_COUNT_LEVEL 5
static void oplus_reverse_clear_keep_status_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, reverse_clear_keep_status_work);

	if (!chip->reverse_enable) {
		chip->req_volt = 0;
		chip->req_current = 0;
		chip->hard_reset_count = 0;
		chip->pre_pdo_voltage = 0;
		chip->pre_pdo_current = 0;
		chip->high_reverse_count = 0;
		chip->drop_down_count = 0;
		vote(chip->reverse_pdo_votable, HW_ERR_VOTER, false, 0, false);
		vote(chip->high_reverse_disable_votable, HW_ERR_VOTER, false, 0, false);
		vote(chip->high_reverse_disable_votable, USB_VOTER, false, 0, false);
		vote(chip->reverse_pdo_votable, CURR_ERR_VOTER, false, 0, false);
		vote(chip->reverse_pdo_votable, USB_VOTER, false, 0, false);
		vote(chip->high_reverse_disable_votable, CURR_ERR_VOTER, false, 0, false);
		vote(chip->reverse_pdo_votable, VOL_DIFF_VOTER, false, 0, false);
		vote(chip->reverse_pdo_votable, VBUS_MV_VOTER, false, 0, false);
	}
}

static void oplus_reverse_chg_test_mode_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, reverse_chg_test_mode_work);
	int rc;
	int pass_retry_cnt = 10;
	int vbus_mv = 0;

	/* system shoud stay awake when testing */
	oplus_reverse_set_awake(chip, true);

	chip->test_high_pwr_vbus_mv = 0;
	chip->test_result = REVERSE_TEST_MODE_RESULT_FAIL;
	rc = oplus_chg_ic_func(g_rvs_chg_chip->reverse_ic,
					OPLUS_IC_FUNC_RVS_SET_HIGH_PWR_MODE_EN, 1);
	do {
		msleep(100);
		vbus_mv = oplus_wired_get_vbus();
		pass_retry_cnt--;
	} while (vbus_mv < 4500 && pass_retry_cnt > 0);
	chip->test_high_pwr_vbus_mv = vbus_mv;
	if (vbus_mv < 4500) {
		rc = oplus_chg_ic_func(g_rvs_chg_chip->reverse_ic,
						OPLUS_IC_FUNC_RVS_SET_HIGH_PWR_MODE_EN, 0);
		chip->reverse_chg_testing = false;
		chip->test_result = REVERSE_TEST_MODE_OPEN_HIGH_PWR_MODE_FAIL;
		/* set awake false after testing complete*/
		oplus_reverse_set_awake(chip, false);
		return;
	}

	pass_retry_cnt = 20;
	do {
		rc = oplus_chg_ic_func(g_rvs_chg_chip->reverse_ic,
						OPLUS_IC_FUNC_RVS_SET_HIGH_PWR_MODE_EN, 0);
		msleep(100);
		pass_retry_cnt--;
		vbus_mv = oplus_wired_get_vbus();
	} while (vbus_mv > CANNOT_OPEN_HIGH_PWR_PATH_VOL_MV && pass_retry_cnt > 0);
	if (vbus_mv < CANNOT_OPEN_HIGH_PWR_PATH_VOL_MV) {
		chip->test_result = REVERSE_TEST_MODE_RESULT_SUCCESS;
	} else {
		chip->test_high_pwr_vbus_mv = vbus_mv;
		chip->test_result = REVERSE_TEST_MODE_CLOSE_HIGH_PWR_MODE_FAIL;
	}

	chip->reverse_chg_testing = false;
	/* set awake false after testing complete*/
	oplus_reverse_set_awake(chip, false);
	return;
}

static int oplus_reverse_temp_cur_range_init(struct oplus_chg_reverse *chip)
{
	int vbat_temp_cur;
	int rc;
	union mms_msg_data data = { 0 };

	rc = oplus_mms_get_item_data(chip->comm_topic, COMM_ITEM_SHELL_TEMP, &data, false);
	if (unlikely(rc < 0)) {
		rc = oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_TEMP, &data, false);
		if (unlikely(rc < 0)) {
			chip->shell_temp = 250;
		} else {
			chip->shell_temp = data.intval;
		}
	} else {
		chip->shell_temp = data.intval;
	}

	vbat_temp_cur = chip->shell_temp;
	chip->ibus_ov = false;
	if (vbat_temp_cur < chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_COLD]) /*0C*/
		chip->reverse_temp_cur_range = TEMP_LEVLE_RANGE_COLD;
	else if (vbat_temp_cur < chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_COOL]) /*0-16C*/
		chip->reverse_temp_cur_range = TEMP_LEVLE_RANGE_COOL;
	else if (vbat_temp_cur < chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_NORMAL]) /*16-45C*/
		chip->reverse_temp_cur_range = TEMP_LEVLE_RANGE_NORMAL;
	else if (vbat_temp_cur < chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_WARM]) /*45-52C*/
		chip->reverse_temp_cur_range = TEMP_LEVLE_RANGE_WARM;
	else
		chip->reverse_temp_cur_range = TEMP_LEVLE_RANGE_HIGH;
	return 0;
}

static void oplus_normal_reverse_set_hw_ocp_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, normal_reverse_set_hw_ocp_work);
	int ocp_count = 0;
	int ocp_level = 0;
	int rc;
	int i;

	if (chip->normal_reverse_count == 0)
		return;

	if (chip->reverse_enable) {
		for (i = 0; i < chip->normal_reverse_count; i++) {
			if (chip->pre_pdo_current != 0 &&
				chip->pre_pdo_current == chip->reverse_limit.normal_reverse_current[i]) {
				ocp_level = chip->reverse_limit.normal_reverse_hw_ocp[i];
				break;
			}
		}
		if (ocp_level != 0) {
			rc = oplus_chg_ic_func(chip->reverse_ic,
					OPLUS_IC_FUNC_SET_NORMAL_REVERSE_HW_OCP,
					ocp_level);
		}
	} else {
		ocp_count = chip->normal_reverse_count - 1;
		rc = oplus_chg_ic_func(chip->reverse_ic,
				OPLUS_IC_FUNC_SET_NORMAL_REVERSE_HW_OCP,
				chip->reverse_limit.normal_reverse_hw_ocp[ocp_count]);
	}
}

static void oplus_get_last_high_reverse_pdo(struct oplus_chg_reverse *chip, int pdo_voltage)
{
	static u32 last_high_reverse_pdo;
	struct mms_msg *msg;
	int rc;

	if (pdo_voltage == NORMAL_VBUS && chip->pre_pdo_voltage > NORMAL_VBUS) {
		last_high_reverse_pdo = chip->pre_pdo_voltage << 16 | chip->pre_pdo_current;
		if (chip->last_high_reverse_pdo != last_high_reverse_pdo) {
			chip->last_high_reverse_pdo = last_high_reverse_pdo;
			msg = oplus_mms_alloc_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
							REVERSE_ITEM_LAST_HIGH_REVERSE_PDO);
			if (msg == NULL) {
				return;
			}
			rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
			if (rc < 0) {
				kfree(msg);
			}
		}
	}
}

static int oplus_reverse_set_pdo(struct oplus_chg_reverse *chip, u32 target_cap_pdo0, u32 target_pdo)
{
	int rc;
	int i;
	int pdo0_volt;
	int pdo0_curr;
	int pdo_voltage;
	int pdo_current;

	pdo0_volt = (target_cap_pdo0 & 0xFFFF0000) >> 16;
	pdo0_curr = (target_cap_pdo0 & 0xFFFF);
	pdo_voltage = (target_pdo & 0xFFFF0000) >> 16;
	pdo_current = (target_pdo & 0xFFFF);

	if (pdo_voltage > NORMAL_VBUS && (pdo0_volt < NORMAL_VBUS || pdo0_curr < LOWER_IBUS)) {
		pdo0_volt = NORMAL_VBUS;
		pdo0_curr = LOWER_IBUS;
	}

	oplus_get_last_high_reverse_pdo(chip, pdo_voltage);

	if (get_client_vote(chip->reverse_pdo_votable, USER_VOTER) > 0) {
		return 0;
	}

	if (get_effective_result(chip->high_reverse_disable_votable) > 0 && pdo_voltage > NORMAL_VBUS) {
		for (i = 0; i < ARRAY_SIZE(disable_vote_level_names); i++) {
			if (get_client_vote(chip->high_reverse_disable_votable, disable_vote_level_names[i]) > 0)
				;
		}
		pdo_voltage = NORMAL_VBUS;
		pdo_current = chip->reverse_soc_pdo_limit[UISOC_LEVEL_MIN].
				reverse_temp_pdo_limit[TEMP_LEVLE_RANGE_COLD].comm_pdo_limit.reverse_pdo_current;
	}
	if (pdo_voltage == chip->pre_pdo_voltage && pdo_current == chip->pre_pdo_current &&
		chip->target_cap_pdo0 == chip->pre_target_cap_pdo0) {
		return 0;
	}

	if (pdo_voltage == NORMAL_VBUS && pdo_current > chip->pre_pdo_current) {
		cancel_delayed_work_sync(&chip->normal_reverse_set_hw_ocp_work);
		chip->pre_pdo_current = pdo_current;
		schedule_delayed_work(&chip->normal_reverse_set_hw_ocp_work, 0);
	}

	if (chip->reverse_ic) {
		rc = oplus_chg_ic_func(chip->reverse_ic,
				OPLUS_IC_FUNC_SET_REVERSE_SRC_PDO,
				pdo0_volt, pdo0_curr, pdo_voltage, pdo_current);
	}

	if (pdo_voltage == NORMAL_VBUS && pdo_current < chip->pre_pdo_current) {
		cancel_delayed_work_sync(&chip->normal_reverse_set_hw_ocp_work);
		schedule_delayed_work(&chip->normal_reverse_set_hw_ocp_work,
			msecs_to_jiffies(REVERSE_ENABLE_DELAY_MS));
	}
	chip->pre_pdo_voltage = pdo_voltage;
	chip->pre_pdo_current = pdo_current;
	chip->pre_target_cap_pdo0 = chip->target_cap_pdo0;
	return 0;
}

PARSE_RESULT parse_numbers(const char *str, int *num1, int *num2, int *num3)
{
	char buffer[32] = {0};
	int parsed  = 0;
	char verify[8] = {0};
	strncpy(buffer, str, sizeof(buffer) - 1);

	parsed = sscanf(buffer, "%7d,%7d,%7d", num1, num2, num3);
	if (parsed != MAX_DIGITS_NUM) {
		return PARSE_FORMAT_ERROR;
	}

	if (snprintf(verify, sizeof(verify), "%d", *num1) > MAX_DIGITS ||
		snprintf(verify, sizeof(verify), "%d", *num2) > MAX_DIGITS ||
		snprintf(verify, sizeof(verify), "%d", *num3) > MAX_DIGITS)
		return PARSE_OVERFLOW_ERROR;

	if (*num1 > MAX_VOLT || *num1 < MIN_VOLT) {
		return PARSE_RANGE_ERROR;
	}

	if (*num2 > MAX_CURRENT || *num2 < MIN_CURRENT) {
		return PARSE_RANGE_ERROR;
	}

	if (*num3 < 0) {
		return PARSE_RANGE_ERROR;
	}

	return PARSE_SUCCESS;
}

int oplus_reverse_high_pwr_test(void)
{
	struct oplus_chg_reverse *chip = g_rvs_chg_chip;
	int rc = 0;
	int hw_detect = 0;
	int vbus_mv = 0;

	if (chip == NULL)
		return -EINVAL;

	vbus_mv = oplus_wired_get_vbus();
	hw_detect = oplus_wired_get_hw_detect();
	if (hw_detect == CC_DETECT_PLUGIN || vbus_mv > CANNOT_OPEN_HIGH_PWR_PATH_VOL_MV) {
		return -EINVAL;
	} else if (hw_detect == CC_DETECT_NULL && vbus_mv > CANNOT_OPEN_HIGH_PWR_PATH_VOL_MV) {
		return -EINVAL;
	} else {
		chip->reverse_chg_testing = true;
		schedule_delayed_work(&chip->reverse_chg_test_mode_work, 0);
	}
	return rc;
}

int oplus_reverse_chg_set_level(const char *buf, int len)
{
	struct oplus_chg_reverse *chip = g_rvs_chg_chip;
	int volt = 0;
	int curr = 0;
	int user_cmd = 0;
	PARSE_RESULT  parse_ret = 0;
	int rc = 0;

	if (chip == NULL)
		return -EINVAL;
	parse_ret = parse_numbers(buf, &volt, &curr, &user_cmd);
	if (parse_ret != PARSE_SUCCESS) {
		return -1;
	}

	switch (user_cmd) {
	case REVERSE_USER_CMD_SET_LEVEL_DEFAULT:
		;
		vote(chip->reverse_pdo_votable, COOL_DOWN_VOTER, true, (volt << 16 | curr), false);
		break;
	case REVERSE_USER_CMD_SET_LEVEL_FORCE:
		;
		if (chip->reverse_pdo_votable)
			vote(chip->reverse_pdo_votable, USER_VOTER, true,
					(volt << 16 | curr), false);
		rc = oplus_reverse_set_source_pdo(chip, NORMAL_VBUS, NORMAL_IBUS, volt, curr);
		if (rc >= 0) {
			chip->pre_pdo_voltage = volt;
			chip->pre_pdo_current = curr;
		}
		break;
	case REVERSE_USER_CMD_SET_HIGH_PWR_PATH_TEST_MODE:
		;
		chip->reverse_chg_test_mode	= 1; /* start test mode*/
		oplus_reverse_high_pwr_test();
		break;
	case REVERSE_USER_CMD_GET_HIGH_PWR_PATH_TEST_END:
		chip->reverse_chg_test_mode = 0; /* end test mode*/
		break;
	default:
		;
		break;
	}
	return 0;
}

#define STR_BUFFER_SIZE 32
int oplus_reverse_chg_info_show(char *buf)
{
	int src_volt = 0;
	int src_current = 0;
	int req_volt = 0;
	int req_current = 0;
	char str_buf[STR_BUFFER_SIZE] = {0};
	int str_len = 0;

	if (g_rvs_chg_chip == NULL)
		return -1;

	src_volt = g_rvs_chg_chip->pre_pdo_voltage;
	src_current = g_rvs_chg_chip->pre_pdo_current;
	req_volt = g_rvs_chg_chip->req_volt;
	req_current = g_rvs_chg_chip->req_current;

	if (g_rvs_chg_chip->reverse_chg_test_mode) {
		req_volt = g_rvs_chg_chip->test_high_pwr_vbus_mv;
		req_current = g_rvs_chg_chip->test_result;
	}
	str_len = snprintf(str_buf, STR_BUFFER_SIZE, "%d+%d+%d+%d",
		src_volt, src_current, req_volt, req_current);

	if (str_len <= 0 || str_len >= (STR_BUFFER_SIZE - 1)) {
		return -1;
	}

	strncpy(buf, str_buf, str_len);
	buf[str_len] = '\0';

	return str_len;
}

#define POWER_ROLE_BUFF_LEN 16
int oplus_reverse_chg_get_power_role_status(char *buf)
{
	struct oplus_chg_reverse *chip = g_rvs_chg_chip;

	if (chip == NULL || buf == NULL)
		return 0;

	return scnprintf(buf, POWER_ROLE_BUFF_LEN, "%d\n", chip->reverse_enable);
}

static int oplus_reverse_set_charging(struct oplus_chg_reverse *chip, bool charging)
{
	struct mms_msg *msg;
	int rc = 0;

	if (chip->high_reverse_charging == charging)
		return 0;

	chip->high_reverse_charging = charging;

	if (!chip->high_reverse_charging)
			chip->reverse_max_curr = 0;
	msg = oplus_mms_alloc_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
					HIGH_REVERSE_ITEM_STATUS);
	if (msg == NULL) {
		return -ENOMEM;
	}
	rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
	if (rc < 0) {
		kfree(msg);
		return rc;
	}


	return 0;
}

static void oplus_reverse_force_exit(struct oplus_chg_reverse *chip)
{
	oplus_reverse_set_charging(chip, false);
}

static bool oplus_check_reverse_chg_source_plug_out(
	struct oplus_chg_reverse *chip)
{
	bool enable = 0;
	int rc;

	if (chip->reverse_ic) {
		rc = oplus_chg_ic_func(chip->reverse_ic,
			OPLUS_IC_FUNC_GET_SOURCE_PLUG_OUT, &enable);
	}

	return enable;
}

static void oplus_reverse_chg_disbale_clear_flags(
	struct oplus_chg_reverse *chip)
{
	vote(chip->high_reverse_disable_votable, BATT_TEMP_VOTER, false, 0, false);
	vote(chip->high_reverse_disable_votable, BATT_SOC_VOTER, false, 0, false);
	vote(chip->high_reverse_disable_votable, CURR_LIMIT_VOTER, false, 0, false);
	vote(chip->high_reverse_disable_votable, USER_VOTER, false, 0, false);
	vote(chip->high_reverse_disable_votable, BATT_VOL_VOTER, false, 0, false);
	vote(chip->reverse_pdo_votable, BATT_TEMP_VOTER, false, 0, false);
	vote(chip->reverse_pdo_votable, BATT_SOC_VOTER, false, 0, false);
	vote(chip->reverse_pdo_votable, CURR_LIMIT_VOTER, false, 0, false);
	vote(chip->reverse_pdo_votable, USER_VOTER, false, 0, false);
	vote(chip->reverse_pdo_votable, BATT_VOL_VOTER, false, 0, false);
	vote(chip->cap_pdo0_votable, BATT_VOL_VOTER, false, 0, false);
	vote(chip->cap_pdo0_votable, BATT_CURR_VOTER, false, 0, false);
	cancel_delayed_work_sync(&chip->reverse_monitor_work);
	oplus_reverse_force_exit(chip);
	schedule_delayed_work(&chip->reverse_chg_type_work, 0);
	schedule_delayed_work(&chip->normal_reverse_set_hw_ocp_work, 0);
	chip->vbus_level = chip->vbus_limit_count;
	chip->vbat_level = chip->vbat_limit_count;
	chip->pre_target_cap_pdo0 = 0;
	chip->kthread_reverse_enable = false;
	chip->high_reverse_enable = false;
}

static void oplus_reverse_online_init(struct oplus_chg_reverse *chip)
{
	chip->last_high_reverse_pdo = 0;
}

static void oplus_reverse_handle_level_for_drop_down(void)
{
	struct oplus_chg_reverse *chip = g_rvs_chg_chip;
	static int pre_drop_down_count = 0;

	if (chip->drop_down_count == pre_drop_down_count
		|| chip->drop_down_count <= 0
		|| chip->drop_down_count <= chip->hard_reset_count
		|| chip->max_reverse_power <= MIN_HIGH_REVERSE_POWER)
		return;

	pre_drop_down_count = chip->drop_down_count;

	if (chip->drop_down_count <= HARD_RESET_COUNT_LEVEL) {
		vote(chip->reverse_pdo_votable, HW_ERR_VOTER, true,
			chip->reverse_vbus_pdo_limit[REVERSE_PDO_SIXTH_LEVEL -
			chip->drop_down_count].reverse_pdo, false);
	} else {
		vote(chip->reverse_pdo_votable, HW_ERR_VOTER, true,
			(NORMAL_VBUS << 16 | NORMAL_IBUS), false);/* 5V1A */
		vote(chip->high_reverse_disable_votable, HW_ERR_VOTER, true, 1, false);
	}
}

static void oplus_reverse_online_work(struct work_struct *work)
{
	struct oplus_chg_reverse *chip =
		container_of(work, struct oplus_chg_reverse, reverse_online_work);

	if (chip->reverse_enable) {
		oplus_reverse_online_init(chip);
		schedule_delayed_work(&chip->reverse_monitor_work, msecs_to_jiffies(2000));
	} else {
		oplus_reverse_chg_disbale_clear_flags(chip);
		if (chip->reverse_ic && chip->use_cp_reverse)
			oplus_reverse_set_source_pdo(chip, NORMAL_VBUS, LOWER_IBUS, NORMAL_VBUS, LOWER_IBUS);
		cancel_delayed_work(&chip->reverse_vbus_retention_check_work);
		schedule_delayed_work(&chip->reverse_vbus_retention_check_work, 0);
		chip->drop_down_count += 1;
		cancel_delayed_work(&chip->reverse_clear_keep_status_work);
		if (chip->max_reverse_power <= MIN_HIGH_REVERSE_POWER) {
			schedule_delayed_work(&chip->reverse_clear_keep_status_work, 0);
		} else {
			schedule_delayed_work(&chip->reverse_clear_keep_status_work, msecs_to_jiffies(5000));
		}
	}

	if (!chip->use_high_reverse_awake) {
		chip->kthread_reverse_enable = chip->reverse_enable;
		if (chip->reverse_enable)
			wake_up_interruptible(&chip->reverse_watchdog_wq);
	}
}

static int oplus_reverse_charge_start(struct oplus_chg_reverse *chip)
{
	int rc;

	if (chip->reverse_enable) {
		rc = oplus_reverse_temp_cur_range_init(chip);
		if (rc < 0)
			return rc;

		oplus_reverse_set_charging(chip, true);
		return 0;
	}

	return -EINVAL;
}

#define HYST 10
static int oplus_reverse_update_batt_temp_curr(struct oplus_chg_reverse *chip)
{
	int ret;
	int vbat_temp_cur;
	int diff;
	int upper_bound;
	int lower_bound;
	union mms_msg_data data = { 0 };
	enum oplus_temp_reason target_temp_range;
	enum oplus_temp_reason current_temp_range;
	enum oplus_temp_reason new_temp_range;

	if (!is_comm_topic_available(chip)) {
		vote(chip->high_reverse_disable_votable, NO_DATA_VOTER, true, 1, false);
		return -ENODEV;
	}

	ret = oplus_mms_get_item_data(chip->comm_topic, COMM_ITEM_SHELL_TEMP, &data, false);
	if (unlikely(ret < 0)) {
		ret = oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_TEMP, &data, false);
		if (unlikely(ret < 0)) {
			chip->shell_temp = 250;
		} else {
			chip->shell_temp = data.intval;
		}
	} else {
		chip->shell_temp = data.intval;
	}

	vbat_temp_cur = chip->shell_temp;

	if (vbat_temp_cur <
		chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_COLD])
		target_temp_range = TEMP_LEVLE_RANGE_COLD;
	else if (vbat_temp_cur <
		chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_COOL])
		target_temp_range = TEMP_LEVLE_RANGE_COOL;
	else if (vbat_temp_cur <
		chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_NORMAL])
		target_temp_range = TEMP_LEVLE_RANGE_NORMAL;
	else if (vbat_temp_cur <
		chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_WARM])
		target_temp_range = TEMP_LEVLE_RANGE_WARM;
	else
		target_temp_range = TEMP_LEVLE_RANGE_HIGH;

	current_temp_range = chip->reverse_temp_cur_range;

	if (current_temp_range < TEMP_LEVLE_RANGE_COLD ||
	    current_temp_range > TEMP_LEVLE_RANGE_HIGH) {
		current_temp_range = TEMP_LEVLE_RANGE_COLD;
		chip->reverse_temp_cur_range = current_temp_range;
	}
	if (target_temp_range < TEMP_LEVLE_RANGE_COLD ||
	    target_temp_range > TEMP_LEVLE_RANGE_HIGH) {
		target_temp_range = TEMP_LEVLE_RANGE_COLD;
	}

	new_temp_range = current_temp_range;
	diff = (int)target_temp_range - (int)current_temp_range;

	if (diff == 1) {
		upper_bound = chip->reverse_limit.reverse_temp_limit[current_temp_range];
		if (vbat_temp_cur >= upper_bound + HYST)
			new_temp_range = target_temp_range;
	}
	else if (diff == -1) {
		lower_bound = chip->reverse_limit.reverse_temp_limit[target_temp_range];
		if (vbat_temp_cur <= lower_bound - HYST)
			new_temp_range = target_temp_range;
	}
	else if (diff > 1 || diff < -1) {
		new_temp_range = target_temp_range;
	}

	if (new_temp_range != current_temp_range)
		chip->reverse_temp_cur_range = new_temp_range;


	return ret;
}

static int oplus_reverse_get_uisoc_curr(struct oplus_chg_reverse *chip)
{
	int ret;
	int ui_soc_cur;
	union mms_msg_data data = { 0 };

	if (!is_comm_topic_available(chip)) {
		vote(chip->high_reverse_disable_votable, NO_DATA_VOTER, true, 1, false);
		return -ENODEV;
	}

	ret = oplus_mms_get_item_data(chip->comm_topic, COMM_ITEM_UI_SOC, &data, false);
	if (ret < 0)
		;
	else
		chip->ui_soc = data.intval;

	ui_soc_cur = chip->ui_soc;

	if (ui_soc_cur < chip->reverse_limit.reverse_uisoc_limit[UISOC_LEVEL_MIN] &&
		ui_soc_cur >= chip->reverse_limit.last_uisoc_limit[LAST_LEVEL_2]) {				/* 20-30%*/
		chip->reverse_uisoc_cur_range = UISOC_LEVEL_MIN;
	} else if (ui_soc_cur <
		chip->reverse_limit.reverse_uisoc_limit[UISOC_LEVEL_MID]) {
		chip->reverse_uisoc_cur_range = UISOC_LEVEL_MID;
	} else if (ui_soc_cur <
		chip->reverse_limit.reverse_uisoc_limit[UISOC_LEVEL_NORMAL]) {
		chip->reverse_uisoc_cur_range = UISOC_LEVEL_NORMAL;
	} else if (ui_soc_cur <
		chip->reverse_limit.reverse_uisoc_limit[UISOC_LEVEL_MID_HIGH]) {
		chip->reverse_uisoc_cur_range = UISOC_LEVEL_MID_HIGH;
	} else if (ui_soc_cur >=
		chip->reverse_limit.reverse_uisoc_limit[UISOC_LEVEL_MID_HIGH]) {
		chip->reverse_uisoc_cur_range = UISOC_LEVEL_HIGH;
	} else {
		chip->reverse_uisoc_cur_range = UISOC_LEVEL_INVALID;
	}


	return ret;
}

#define OPLUS_SVID 		    0x22D9
static void oplus_update_pdo_limit(struct oplus_chg_reverse *chip)
{
	u32 pdo_limit;
	int soc_idx = chip->reverse_uisoc_cur_range;
	int temp_idx = chip->reverse_temp_cur_range;

	if (soc_idx < 0 || soc_idx >= ARRAY_SIZE(chip->reverse_soc_pdo_limit)) {
		return;
	}
	if (temp_idx < 0 ||
		temp_idx >= ARRAY_SIZE(chip->reverse_soc_pdo_limit[UISOC_LEVEL_MIN].reverse_temp_pdo_limit)) {
		return;
	}

	if (chip->sink_svid_support && chip->reverse_chg_svid == OPLUS_SVID)
		pdo_limit = chip->reverse_soc_pdo_limit[soc_idx].
				reverse_temp_pdo_limit[temp_idx].oplus_pdo_limit.reverse_pdo;
	else
		pdo_limit = chip->reverse_soc_pdo_limit[soc_idx].
				reverse_temp_pdo_limit[temp_idx].comm_pdo_limit.reverse_pdo;
	vote(chip->reverse_pdo_votable, BATT_SOC_VOTER, true, pdo_limit, false);

	if ((pdo_limit & 0xFFFF0000) >> 16 == NORMAL_VBUS) {
		if (temp_idx <= TEMP_LEVLE_RANGE_COLD ||
		    temp_idx >= TEMP_LEVLE_RANGE_WARM)
			vote(chip->high_reverse_disable_votable, BATT_TEMP_VOTER, true, 1, false);
		if (soc_idx == UISOC_LEVEL_MIN || soc_idx == UISOC_LEVEL_INVALID)
			vote(chip->high_reverse_disable_votable, BATT_SOC_VOTER, true, 1, false);
	} else {
		vote(chip->high_reverse_disable_votable, BATT_TEMP_VOTER, false, 0, false);
		vote(chip->high_reverse_disable_votable, BATT_SOC_VOTER, false, 0, false);
	}

}

static int oplus_reverse_charge_allow_check(struct oplus_chg_reverse *chip)
{
	union mms_msg_data data = { 0 };
	int chg_temp = 0;
	int rc = 0;

	rc = oplus_mms_get_item_data(chip->comm_topic, COMM_ITEM_SHELL_TEMP, &data, true);
	if (unlikely(rc < 0)) {
		rc = oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_TEMP, &data, false);
		if (unlikely(rc < 0)) {
			chg_temp = 250;
		} else {
			chg_temp = data.intval;
		}
	} else {
		chg_temp = data.intval;
	}

	if (chip->max_reverse_power <= MIN_HIGH_REVERSE_POWER)
		return 0;

	/*chg_temp over_low or over_high vote*/
	if (chg_temp < chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_COLD]) {
		vote(chip->high_reverse_disable_votable, BATT_TEMP_VOTER, true, 1, false);
	} else if (chg_temp >= chip->reverse_limit.reverse_temp_limit[TEMP_LEVLE_RANGE_WARM]) {
		vote(chip->high_reverse_disable_votable, BATT_TEMP_VOTER, true, 1, false);
	} else {
		vote(chip->high_reverse_disable_votable, BATT_TEMP_VOTER, false, 0, false);
	}

	return 0;
}

static int oplus_reverse_get_boost_curr(struct oplus_chg_reverse *chip, int *iout)
{
	int rc = -1;
	if (iout == NULL)
		return rc;

	if (chip->use_cp_reverse && chip->cp_ic) {
		rc = oplus_chg_ic_func(chip->cp_ic, OPLUS_IC_FUNC_CP_GET_IOUT, iout);
	} else {
		*iout = oplus_gauge_get_batt_current();
		rc = *iout < 0 ? -1 : 0;
	}

	return rc;
}

static int oplus_reverse_get_boost_vol(
	struct oplus_chg_reverse *chip, int *vout)
{
	int rc = -1;
	if (vout == NULL)
		return rc;

	if (chip->use_cp_reverse && chip->cp_ic) {
		rc = oplus_chg_ic_func(chip->cp_ic, OPLUS_IC_FUNC_CP_GET_REVERSE_VOUT, vout);
	} else {
		*vout = oplus_wired_get_vbus();
		rc = *vout < 0 ? -1 : 0;
	}

	return rc;
}

static int  oplus_reverse_check_ibus_curr(struct oplus_chg_reverse *chip)
{
#define REVERSE_IBUS_OVER_COUNTS 3
#define REVERSE_TBATT_OV_CNT	1
#define OCP_ADD_LEVEL	300
#define OCBAT_ADD_LEVEL		3000

	int ret = 0;
	int cp_ibus_ma;
	int cp_vbus_mv;
	int over_curr_offset = 0;
	int batt_num = oplus_gauge_get_batt_num();

	if (!chip->reverse_enable) {
		chip->ibus_over = 0;
		chip->ibus_lower = 0;
		return -EINVAL;
	}

	if (batt_num <= 0 || batt_num > 2)
		batt_num = 1;

	if (chip->max_reverse_power <= MIN_HIGH_REVERSE_POWER)
		return -EINVAL;

	ret = oplus_reverse_get_boost_curr(chip, &cp_ibus_ma);
	ret = oplus_reverse_get_boost_vol(chip, &cp_vbus_mv);


	if (cp_vbus_mv > HIGH_VBUS_START) {
		if (cp_ibus_ma < chip->reverse_ibus_lower_oplus) {
			chip->ibus_lower++;
			if (chip->ibus_lower > REVERSE_IBUS_OVER_COUNTS) {
				vote(chip->reverse_pdo_votable, CURR_ERR_VOTER,
					true, chip->over_ibus_pdo_limit, false);/* 5V1.5A */
				vote(chip->high_reverse_disable_votable, CURR_ERR_VOTER, true, 1, false);
				chip->ibus_lower = 0;
			}
		} else {
			chip->ibus_lower = 0;
		}

		if (chip->use_cp_reverse)
			over_curr_offset = OCP_ADD_LEVEL;
		else
			over_curr_offset = OCBAT_ADD_LEVEL;

		if (cp_ibus_ma > over_curr_offset + (chip->pre_pdo_current * (2 / batt_num))) {
			chip->ibus_over++;
			if (chip->ibus_over > REVERSE_IBUS_OVER_COUNTS) {
				vote(chip->reverse_pdo_votable, CURR_ERR_VOTER,
					true, chip->over_ibus_pdo_limit, false);/* 5V1.5A */
				vote(chip->high_reverse_disable_votable, CURR_ERR_VOTER, true, 1, false);
				chip->ibus_over = 0;
			}
		} else {
			chip->ibus_over = 0;
		}
	}

	return ret;
}

#define FORCE_EXIT_REVERSE_CHG_VBUS_THR  2000
static void oplus_reverse_vbus_hw_detect_check(struct oplus_chg_reverse *chip)
{
	int vbus_check = 0;
	int hw_detect_check = 0;
	int retry_check = 5; /*retry 5 count*/

	do {
		vbus_check = oplus_wired_get_vbus();
		hw_detect_check = oplus_wired_get_hw_detect();
		if (vbus_check > FORCE_EXIT_REVERSE_CHG_VBUS_THR
			|| hw_detect_check == CC_DETECT_PLUGIN) {
			break;
		}
		msleep(500);
		retry_check--;
	} while (retry_check > 0);

	if (retry_check <= 0
		&& vbus_check <= FORCE_EXIT_REVERSE_CHG_VBUS_THR
		&& hw_detect_check != CC_DETECT_PLUGIN) {
		chip->reverse_enable = false;
		schedule_work(&chip->reverse_online_work);
	}

	return;
}

static void oplus_reverse_vbus_retention_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, reverse_vbus_retention_check_work);
	int i = 0;
	int retry_check = 5;
	static int vbus_retention_pdo_level;
	static int vbus_down_count = 0;

	for (i = 0; i < retry_check; i++) {
		msleep(500);
		if (chip->reverse_enable)
			break;
	}
	if (chip->reverse_enable && chip->req_volt == PD_VBUS) {
		vbus_down_count++;
		vbus_retention_pdo_level = chip->reverse_vbus_pdo_limit_num - vbus_down_count;
		if (vbus_down_count < chip->reverse_vbus_pdo_limit_num) {
			for (i = vbus_retention_pdo_level; i > 1; i--) {
				if (chip->reverse_vbus_pdo_limit[vbus_retention_pdo_level].reverse_pdo
					> ((chip->pre_pdo_voltage << 16) | chip->pre_pdo_current)) {
					vbus_retention_pdo_level--;
				} else {
					break;
				}
			}
			if (vbus_retention_pdo_level >= 1)
				vote(chip->reverse_pdo_votable, VBUS_MV_VOTER, true,
					chip->reverse_vbus_pdo_limit[vbus_retention_pdo_level - 1].reverse_pdo, false);
			else
				vote(chip->reverse_pdo_votable, VBUS_MV_VOTER, true,
					chip->reverse_vbus_pdo_limit[0].reverse_pdo, false);
		}
	} else {
		vbus_down_count = 0;
		vote(chip->reverse_pdo_votable, VBUS_MV_VOTER, false, 0, false);
	}
	return;
}

#define OPLUS_SVID 		    0x22D9
static void oplus_reverse_protection_check(struct oplus_chg_reverse *chip, struct puc_strategy_ret_data *data)
{
	int rc;
	struct mms_msg *msg;
	/* authenticate or hmac vote */
	if ((!chip->authenticate || !chip->hmac) && (chip->max_reverse_power > MIN_HIGH_REVERSE_POWER)) {
		vote(chip->reverse_pdo_votable, AUTH_VOTER,
			true, chip->authenticate_pdo_limit, false);/* 5V1.5A */
		vote(chip->high_reverse_disable_votable, AUTH_VOTER, true, 1, false);
	} else {
		vote(chip->high_reverse_disable_votable, AUTH_VOTER, false, 0, false);
		vote(chip->reverse_pdo_votable, AUTH_VOTER, false, 0, false);
	}

	/* Power Saving Mode and Ultra Power Saving Mode vote */
	if (chip->ui_soc < chip->reverse_limit.last_uisoc_limit[LAST_LEVEL_2]) {
		if (chip->ui_soc <= chip->reverse_limit.last_uisoc_limit[LAST_LEVEL_1]) {
			vote(chip->reverse_pdo_votable, CURR_LIMIT_VOTER, true,
				(NORMAL_VBUS << 16 | chip->reverse_limit.last_current[LAST_LEVEL_1]), false);/* 5V 0.5A */
			vote(chip->high_reverse_disable_votable, CURR_LIMIT_VOTER, true, 0, false);
		} else {
			vote(chip->reverse_pdo_votable, CURR_LIMIT_VOTER, true,
				(NORMAL_VBUS << 16 | chip->reverse_limit.last_current[LAST_LEVEL_2]), false);/* 5V1.5A */
			vote(chip->high_reverse_disable_votable, CURR_LIMIT_VOTER, true, 0, false);
		}
	}

	/* led_on vote */
	if (chip->led_on && !get_effective_result(chip->high_reverse_disable_votable) &&
		(chip->led_on_pdo_limit > 0)) {
		vote(chip->reverse_pdo_votable, LED_ON_VOTER, true, chip->led_on_pdo_limit, false);/* 9V1A */
	} else {
		vote(chip->reverse_pdo_votable, LED_ON_VOTER, false, 0, false);
	}

	/* SVID vote */
	rc = oplus_chg_ic_func(chip->reverse_ic,
		OPLUS_IC_FUNC_GET_REVERSE_CHG_SVID, &chip->reverse_chg_svid);
	if (rc >= 0) {
		if (chip->sink_svid_support && chip->reverse_chg_svid != OPLUS_SVID &&
			chip->ui_soc < chip->reverse_limit.reverse_uisoc_limit[UISOC_LEVEL_NORMAL]) {
			vote(chip->reverse_pdo_votable, SVID_VOTER, true,
				NORMAL_VBUS << 16 | chip->thired_oplus_svid_limit, false);/* 5V1.5A */
			vote(chip->high_reverse_disable_votable, SVID_VOTER, true, 1, false);
		} else {
			vote(chip->high_reverse_disable_votable, SVID_VOTER, false, 0, false);
			vote(chip->reverse_pdo_votable, SVID_VOTER, false, 0, false);
		}
		if (chip->reverse_chg_svid != chip->oplus_svid) {
			chip->oplus_svid = (int)chip->reverse_chg_svid;
			msg = oplus_mms_alloc_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
							REVERSE_ITEM_SINK_OPLUS_SVID);
			if (msg == NULL) {
				return;
			}
			rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
			if (rc < 0) {
				kfree(msg);
			}
		}
	} else
		;

	oplus_reverse_check_ibus_curr(chip);
	oplus_reverse_vbus_hw_detect_check(chip);
	schedule_delayed_work(&chip->usbtemp_high_work, 0);
	schedule_work(&chip->reverse_vbus_check_work);
}

#define WAIT_VBUS_READY_TIME 1000
static void oplus_reverse_vbus_check_work(struct work_struct *work)
{
	struct oplus_chg_reverse *chip =
		container_of(work, struct oplus_chg_reverse, reverse_vbus_check_work);
	int ret;
	int reverse_vbus_mv;

	if (chip->vbus_level == 0 ||
		chip->reverse_vbus_pdo_limit_num != chip->vbus_limit_count + 1) {
		return;
	}

	ret = oplus_reverse_get_boost_vol(chip, &reverse_vbus_mv);
	if (ret < 0) {
		return;
	}
	if (chip->vbus_level > 0 &&
		chip->pre_pdo_voltage > NORMAL_VBUS && reverse_vbus_mv > HIGH_VBUS_START &&
		reverse_vbus_mv <
		chip->reverse_limit.reverse_vbus_limit[chip->vbus_level - 1]) {
		do {
			if (reverse_vbus_mv <
				chip->reverse_limit.reverse_vbus_limit[chip->vbus_level - 1]) {
				vote(chip->reverse_pdo_votable, VOL_DIFF_VOTER, true,
					chip->reverse_vbus_pdo_limit[chip->vbus_level - 1].reverse_pdo, false);
				mutex_lock(&chip->set_pdo_lock);
				oplus_reverse_set_pdo(chip, chip->target_cap_pdo0, chip->target_pdo);
				mutex_unlock(&chip->set_pdo_lock);
			} else {
				break;
			}
			msleep(WAIT_VBUS_READY_TIME);
			oplus_reverse_get_boost_vol(chip, &reverse_vbus_mv);
			chip->vbus_level--;
		} while (chip->vbus_level >= 1);
	}
	return;
}

#define WAIT_VBAT_READY_TIME 1000
static void oplus_reverse_vbat_check(struct oplus_chg_reverse *chip)
{
	union mms_msg_data data = { 0 };

	if (chip->vbat_level == 0 ||
		chip->normal_reverse_count != (chip->vbat_limit_count + 1)) {
		return;
	}
	oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_VOL_MAX, &data, true);
	chip->vbat_mv = data.intval;

	if (chip->vbat_level > 0 &&
		chip->vbat_mv < chip->reverse_limit.reverse_vbat_limit[chip->vbat_level - 1]) {
		do {
			if (chip->vbat_mv < chip->reverse_limit.reverse_vbat_limit[chip->vbat_level - 1]) {
				if (chip->pre_pdo_voltage == NORMAL_VBUS || !chip->is_support_high_reverse) {
					vote(chip->reverse_pdo_votable, BATT_VOL_VOTER, true,
					(NORMAL_VBUS << 16 |
					chip->reverse_limit.normal_reverse_current[chip->vbat_level - 1]),
					false);
				} else if (chip->pre_pdo_voltage > NORMAL_VBUS || chip->pre_pdo_voltage == 0) {
					vote(chip->cap_pdo0_votable, BATT_VOL_VOTER, true,
					(NORMAL_VBUS << 16 |
					chip->reverse_limit.normal_reverse_current[chip->vbat_level - 1]),
					false);
				}
				mutex_lock(&chip->set_pdo_lock);
				oplus_reverse_set_pdo(chip, chip->target_cap_pdo0, chip->target_pdo);
				mutex_unlock(&chip->set_pdo_lock);
			} else {
				break;
			}
			msleep(WAIT_VBAT_READY_TIME);
			oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_VOL_MAX, &data, true);
			chip->vbat_mv = data.intval;
			chip->vbat_level--;
		} while (chip->vbat_level >= 1);
	} else if (chip->vbat_level > 0 && chip->pre_pdo_voltage > NORMAL_VBUS &&
			chip->vbat_mv >= chip->reverse_limit.reverse_vbat_limit[chip->vbat_limit_count - 1]) {
			vote(chip->cap_pdo0_votable, BATT_CURR_VOTER, true,
				(NORMAL_VBUS << 16 |
				chip->reverse_limit.normal_reverse_current[chip->normal_reverse_count - 1]),
				false);
			mutex_lock(&chip->set_pdo_lock);
			oplus_reverse_set_pdo(chip, chip->target_cap_pdo0, chip->target_pdo);
			mutex_unlock(&chip->set_pdo_lock);
	} else {
		mutex_lock(&chip->set_pdo_lock);
		oplus_reverse_set_pdo(chip, chip->target_cap_pdo0, chip->target_pdo);
		mutex_unlock(&chip->set_pdo_lock);
	}
	return;
}

#define USER_CONTROL_MAX_LEVEL 6 /* 30S */
static void oplus_reverse_monitor_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip =
		container_of(dwork, struct oplus_chg_reverse, reverse_monitor_work);
	struct puc_strategy_ret_data data;
	static int user_voter_control;
	int rc;
	int range_switch_dealy = HRP_TEMP_SWITCH_DELAY;

	if (!chip->high_reverse_charging) {
		rc = oplus_reverse_charge_start(chip);
		if (rc < 0)
			return;
	}

	oplus_reverse_handle_level_for_drop_down();
	oplus_reverse_protection_check(chip, &data);

	if (!chip->reverse_enable) {
		oplus_reverse_force_exit(chip);
	}
	oplus_reverse_charge_allow_check(chip);
	/* ui_soc vote*/
	oplus_reverse_get_uisoc_curr(chip);
	/* temp vote*/
	rc = oplus_reverse_update_batt_temp_curr(chip);
	if (rc < 0)
		goto exit;
	oplus_update_pdo_limit(chip);

	if (chip->req_volt == PD_VBUS)
		schedule_delayed_work(&chip->reverse_error_flag_work, 0);

	schedule_delayed_work(&chip->reverse_chg_type_work, 0);

	/* AT vote*/
	if (get_client_vote(chip->reverse_pdo_votable, USER_VOTER) > 0) {
		user_voter_control++;
		if (user_voter_control >= USER_CONTROL_MAX_LEVEL) {
			user_voter_control = 0;
			vote(chip->reverse_pdo_votable, USER_VOTER, false, 0, false);
		}
	}

	if (chip->vbat_level != 0 &&
		chip->normal_reverse_count == (chip->vbat_limit_count + 1)) {
		oplus_reverse_vbat_check(chip);
	} else {
		mutex_lock(&chip->set_pdo_lock);
		oplus_reverse_set_pdo(chip, chip->target_cap_pdo0, chip->target_pdo);
		mutex_unlock(&chip->set_pdo_lock);
	}

	schedule_delayed_work(&chip->reverse_monitor_work, msecs_to_jiffies(REVERSE_ENABLE_DELAY_MS));
	return;
exit:
	schedule_delayed_work(&chip->reverse_monitor_work, msecs_to_jiffies(range_switch_dealy));
	return;
}
static int oplus_high_reverse_charging_status(struct oplus_mms *mms,
					    union mms_msg_data *data)
{
	struct oplus_chg_reverse *chip;

	if (mms == NULL) {
		return -EINVAL;
	}
	if (data == NULL) {
		return -EINVAL;
	}

	chip = oplus_mms_get_drvdata(mms);
	if (!chip) {
		return -EINVAL;
	}

	data->intval = chip->high_reverse_charging;
	return 0;
}

static void oplus_reverse_awake_init(struct oplus_chg_reverse *chip)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	wake_lock_init(&chip->reverse_wake_lock, WAKE_LOCK_SUSPEND,
			"reverse_wake_lock");
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 102) &&                      \
		LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 999))
	chip->reverse_ws = wakeup_source_register("reverse_wake_lock");
#else
	chip->reverse_ws = wakeup_source_register(NULL, "reverse_wake_lock");
#endif
}

static void oplus_reverse_awake_exit(struct oplus_chg_reverse *chip)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	wake_lock_destroy(&chip->reverse_wake_lock);
#else
	wakeup_source_unregister(chip->reverse_ws);
#endif
}

static void oplus_reverse_set_awake(struct oplus_chg_reverse *chip, bool awake)
{
	static bool pm_flag = false;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	if (awake && !pm_flag) {
		pm_flag = true;
		wake_lock(&chip->reverse_wake_lock);
	} else if (!awake && pm_flag) {
		wake_unlock(&chip->reverse_wake_lock);
		pm_flag = false;
	}
#else
	if (!chip || !chip->reverse_ws) {
		return;
	}
	if (awake && !pm_flag) {
		pm_flag = true;
		__pm_stay_awake(chip->reverse_ws);
	} else if (!awake && pm_flag) {
		__pm_relax(chip->reverse_ws);
		pm_flag = false;
	}
#endif
}

static int oplus_chg_reverse_watchdog_kthread(void *arg)
{
	struct oplus_chg_reverse *chip = (struct oplus_chg_reverse *)arg;
	int rc;

	while (!kthread_should_stop()) {
		wait_event_interruptible(chip->reverse_watchdog_wq, chip->kthread_reverse_enable);
		oplus_reverse_set_awake(chip, true);
		if (chip->use_cp_reverse && chip->cp_ic && chip->high_reverse_enable) {
			rc = oplus_chg_ic_func(chip->cp_ic, OPLUS_IC_FUNC_CP_WATCHDOG_RESET);
			if (rc < 0 && rc != -ENOTSUPP) {
				oplus_reverse_set_awake(chip, false);
				return rc;
			}
		}
		schedule_timeout_interruptible(round_jiffies_relative(msecs_to_jiffies(chip->reset_watchdog_time)));
		oplus_reverse_set_awake(chip, false);
	}
	return 0;
}

static void oplus_chg_reverse_watchdog_init(struct oplus_chg_reverse *chip)
{
	int rc;

	init_waitqueue_head(&chip->reverse_watchdog_wq);

	chip->reverse_watchdog = kthread_run(
		oplus_chg_reverse_watchdog_kthread, chip, "reverse_watchdog");
	if (!IS_ERR(chip->reverse_watchdog)) {
	} else {
		rc = PTR_ERR(chip->reverse_watchdog);
	}
	return;
}

static int oplus_high_reverse_chg_enable_status(struct oplus_mms *mms,
						    union mms_msg_data *data)
{
	struct oplus_chg_reverse *chip;
	bool enable = false;

	if (mms == NULL) {
		return -EINVAL;
	}
	if (data == NULL) {
		return -EINVAL;
	}
	chip = oplus_mms_get_drvdata(mms);

	enable = chip->high_reverse_enable;
	if (chip->use_high_reverse_awake) {
		chip->kthread_reverse_enable = chip->high_reverse_enable;
		if (enable)
			wake_up_interruptible(&chip->reverse_watchdog_wq);
	}
	data->intval = enable;
	return 0;
}

static int oplus_reverse_chg_type_update(struct oplus_mms *mms,
						    union mms_msg_data *data)
{
	struct oplus_chg_reverse *chip;

	if (mms == NULL) {
		return -EINVAL;
	}
	if (data == NULL) {
		return -EINVAL;
	}
	chip = oplus_mms_get_drvdata(mms);
	data->intval = chip->reverse_chg_type;
	return 0;
}

static int oplus_reverse_oplus_svid_update(struct oplus_mms *mms,
						    union mms_msg_data *data)
{
	struct oplus_chg_reverse *chip;

	if (mms == NULL) {
		return -EINVAL;
	}
	if (data == NULL) {
		return -EINVAL;
	}
	chip = oplus_mms_get_drvdata(mms);
	data->intval = chip->oplus_svid;
	return 0;
}

static int oplus_reverse_high_reverse_err(struct oplus_mms *mms,
						    union mms_msg_data *data)
{
	struct oplus_chg_reverse *chip;

	if (mms == NULL) {
		return -EINVAL;
	}
	if (data == NULL) {
		return -EINVAL;
	}
	chip = oplus_mms_get_drvdata(mms);
	data->intval = chip->high_reverse_err_flag;
	return 0;
}

static int oplus_high_reverse_chg_count(struct oplus_mms *mms,
						    union mms_msg_data *data)
{
	struct oplus_chg_reverse *chip;

	if (mms == NULL) {
		return -EINVAL;
	}
	if (data == NULL) {
		return -EINVAL;
	}
	chip = oplus_mms_get_drvdata(mms);
	data->intval = chip->high_reverse_count;
	return 0;
}
static int oplus_last_high_reverse_pdo(struct oplus_mms *mms,
						    union mms_msg_data *data)
{
	struct oplus_chg_reverse *chip;

	if (mms == NULL) {
		return -EINVAL;
	}
	if (data == NULL) {
		return -EINVAL;
	}
	chip = oplus_mms_get_drvdata(mms);
	data->intval = chip->last_high_reverse_pdo;
	return 0;
}

static void oplus_reverse_chg_online_handler(struct oplus_chg_ic_dev *ic_dev,
					   void *virq_data)
{
}

static void oplus_high_reverse_chg_offline_handler(struct oplus_chg_ic_dev *ic_dev,
					   void *virq_data)
{
}

static void oplus_high_reverse_chg_err_handler(struct oplus_chg_ic_dev *ic_dev,
					   void *virq_data)
{
}

static void oplus_reverse_chg_enable_handler(struct oplus_chg_ic_dev *ic_dev,
					   void *virq_data)
{
	struct oplus_chg_reverse *chip = virq_data;
	int rc;
	bool enable;

	rc = oplus_chg_ic_func(chip->reverse_ic,
			       OPLUS_IC_FUNC_GET_REVERSE_ENABLE, &enable);
	if (rc < 0)
		enable = false;
	chip->reverse_enable = enable;
	schedule_work(&chip->reverse_online_work);
}

static void oplus_high_reverse_chg_enable_handler(struct oplus_chg_ic_dev *ic_dev,
					   void *virq_data)
{
	struct oplus_chg_reverse *chip = virq_data;
	int rc;
	struct mms_msg *msg;
	bool enable;

	rc = oplus_chg_ic_func(chip->reverse_ic,
			       OPLUS_IC_FUNC_GET_HIGH_REVERSE_ENABLE, &enable);
	if (rc < 0)
		enable = false;
	chip->high_reverse_enable = enable;
	if (enable)
		chip->high_reverse_count++;
	msg = oplus_mms_alloc_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
					REVERSE_ITEM_HIGH_REVERSE_CHG_ENABLE);
	if (msg == NULL) {
		return;
	}
	rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
	if (rc < 0) {
		kfree(msg);
	}
	msg = oplus_mms_alloc_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
					REVERSE_ITEM_HIGH_REVERSE_CHG_COUNT);
	if (msg == NULL) {
		return;
	}
	rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
	if (rc < 0) {
		kfree(msg);
	}
}

static void oplus_reverse_hard_reset_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, reverse_hard_reset_work);
	static int hard_reset_pdo_level;
	int i;

	if (chip->hardreset_limit_count == 0) {
		return;
	}
	if (chip->req_volt != PD_VBUS) {
		return;
	}
	if (!chip->reverse_enable)
		msleep(1000); /*wait reverse enable*/
	if (chip->reverse_enable) {
		chip->hard_reset_count++;
		hard_reset_pdo_level = chip->hardreset_limit_count - chip->hard_reset_count;
		if (chip->hard_reset_count < chip->hardreset_limit_count && chip->pre_pdo_voltage == PD_VBUS) {
			for (i = hard_reset_pdo_level; i > 1; i--) {
				if (chip->reverse_hardreset_pdo_limit[hard_reset_pdo_level].reverse_pdo
					> ((chip->pre_pdo_voltage << 16) | chip->pre_pdo_current))
					hard_reset_pdo_level--;
				else
					break;
			}
			if (hard_reset_pdo_level >= 1)
				vote(chip->reverse_pdo_votable, HW_ERR_VOTER, true,
					chip->reverse_hardreset_pdo_limit[hard_reset_pdo_level - 1].reverse_pdo, false);
			else
				vote(chip->reverse_pdo_votable, HW_ERR_VOTER, true,
					chip->reverse_hardreset_pdo_limit[0].reverse_pdo, false);
		}
	}
	if (chip->hard_reset_count >= chip->hardreset_limit_count) {
		vote(chip->reverse_pdo_votable, HW_ERR_VOTER, true,
			(NORMAL_VBUS << 16 | NORMAL_IBUS), false);/* 5V1A */
		vote(chip->high_reverse_disable_votable, HW_ERR_VOTER, true, 1, false);
		chip->hard_reset_count = 0;
	}
}

static void oplus_reverse_hard_reset_handler(struct oplus_chg_ic_dev *ic_dev,
					   void *virq_data)
{
	struct oplus_chg_reverse *chip = virq_data;
	schedule_delayed_work(&chip->reverse_hard_reset_work, 0);
}

static void oplus_reverse_sink_req_msg_handler(struct oplus_chg_ic_dev *ic_dev,
					   void *virq_data)
{
	struct oplus_chg_reverse *chip = virq_data;
	struct mms_msg *msg;
	int msg_type = REVERSE_CHG_MSG_TYPE_UNKNOWN;
	int req_voltage = 0;
	int req_current = 0;
	int rc;

	rc = oplus_chg_ic_func(chip->reverse_ic,
			       OPLUS_IC_FUNC_GET_RVS_CHG_MSG, &msg_type);
	if (rc < 0) {
		msg_type = REVERSE_CHG_MSG_TYPE_UNKNOWN;
		return;
	}

	switch (msg_type) {
	case REVERSE_CHG_MSG_TYPE_SINK_REQ_PDO:
		rc = oplus_chg_ic_func(chip->reverse_ic,
			       OPLUS_IC_FUNC_GET_SINK_REQ_PDO, &req_voltage, &req_current);
		if (rc >= 0) {
			chip->req_volt = req_voltage;
			chip->req_current = req_current;
		}

		if (req_voltage >= 5000 && req_current > 0) {
			msg = oplus_mms_alloc_int_msg(MSG_TYPE_ITEM, MSG_PRIO_MEDIUM,
						REVERSE_ITEM_SINK_REQUEST_VOLT, chip->req_volt);
			if (msg == NULL) {
				return;
			}
			rc = oplus_mms_publish_msg(chip->reverse_topic, msg);
			if (rc < 0) {
				kfree(msg);
				return;
			}
		}
		break;
	default:
		;
		break;
	}

	return;
}

static void oplus_reverse_topic_update(struct oplus_mms *mms, bool publish)
{
}

static struct mms_item oplus_reverse_item[] = {
	{
		.desc = {
			.item_id = HIGH_REVERSE_ITEM_STATUS,
			.update = oplus_high_reverse_charging_status,
		}
	},
	{
		.desc = {
			.item_id = REVERSE_ITEM_HIGH_REVERSE_CHG_ENABLE,
			.update = oplus_high_reverse_chg_enable_status,
		}
	},
	{
		.desc = {
			.item_id = REVERSE_ITEM_REVERSE_CHG_TYPE,
			.update = oplus_reverse_chg_type_update,
		}
	},
	{
		.desc = {
			.item_id = REVERSE_ITEM_SINK_OPLUS_SVID,
			.update = oplus_reverse_oplus_svid_update,
		}
	},
	{
		.desc = {
			.item_id = REVERSE_ITEM_HIGH_REVERSE_ERR,
			.update = oplus_reverse_high_reverse_err,
		}
	},
	{
		.desc = {
			.item_id = REVERSE_ITEM_HIGH_REVERSE_CHG_COUNT,
			.update = oplus_high_reverse_chg_count,
		}
	},
	{
		.desc = {
			.item_id = REVERSE_ITEM_LAST_HIGH_REVERSE_PDO,
			.update = oplus_last_high_reverse_pdo,
		}
	},
	{
		.desc = {
			.item_id = REVERSE_ITEM_SINK_REQUEST_VOLT,
			.update = NULL,
		}
	}
};

static const struct oplus_mms_desc oplus_reverse_desc = {
	.name = "reverse",
	.type = OPLUS_MMS_TYPE_REVERSE,
	.item_table = oplus_reverse_item,
	.item_num = ARRAY_SIZE(oplus_reverse_item),
	.update_items = NULL,
	.update_items_num = 0,
	.update_interval = 0, /* ms */
	.update = oplus_reverse_topic_update,
};

static void oplus_reverse_pdo_update_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip =
		container_of(dwork, struct oplus_chg_reverse, reverse_pdo_update_work);

	if (chip->led_on_pdo_limit == 0)
		return;

	if (chip->led_on && !get_effective_result(chip->high_reverse_disable_votable))
		vote(chip->reverse_pdo_votable, LED_ON_VOTER, true, chip->led_on_pdo_limit, false);/* 9V1A */
	else
		vote(chip->reverse_pdo_votable, LED_ON_VOTER, false, 0, false);

	mutex_lock(&chip->set_pdo_lock);
	oplus_reverse_set_pdo(chip, chip->target_cap_pdo0, chip->target_pdo);
	mutex_unlock(&chip->set_pdo_lock);
}

static void oplus_reverse_comm_subs_callback(struct mms_subscribe *subs,
					 enum mms_msg_type type, u32 id, bool sync)
{
	struct oplus_chg_reverse *chip = subs->priv_data;
	union mms_msg_data data = { 0 };

	switch (type) {
	case MSG_TYPE_TIMER:
		break;
	case MSG_TYPE_ITEM:
		switch (id) {
		case COMM_ITEM_UI_SOC:
			oplus_mms_get_item_data(chip->comm_topic, id, &data,
						false);
			chip->ui_soc = data.intval;
			break;
		case COMM_ITEM_SHELL_TEMP:
			oplus_mms_get_item_data(chip->comm_topic, id, &data,
						false);
			chip->shell_temp = data.intval;
			break;
		case COMM_ITEM_LED_ON:
			oplus_mms_get_item_data(chip->comm_topic, id, &data,
						false);
			chip->led_on = data.intval;
			if (chip->reverse_enable)
				schedule_delayed_work(&chip->reverse_pdo_update_work, 0);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static void oplus_reverse_subscribe_comm_topic(struct oplus_mms *topic,
					     void *prv_data)
{
	struct oplus_chg_reverse *chip = prv_data;
	union mms_msg_data data = { 0 };

	chip->comm_topic = topic;
	chip->comm_subs =
		oplus_mms_subscribe(topic, chip,
				    oplus_reverse_comm_subs_callback, "reverse");
	if (IS_ERR_OR_NULL(chip->comm_subs)) {
		return;
	}

	oplus_mms_get_item_data(chip->comm_topic, COMM_ITEM_UI_SOC, &data, false);
	chip->ui_soc = data.intval;
	oplus_mms_get_item_data(chip->comm_topic, COMM_ITEM_SHELL_TEMP, &data, false);
	chip->shell_temp = data.intval;
	oplus_mms_get_item_data(chip->comm_topic, COMM_ITEM_LED_ON, &data, false);
	chip->led_on = data.intval;

}

static bool is_sub_gauge_topic_available(struct oplus_chg_reverse *chip)
{
	if (!chip->sub_gauge_topic)
		chip->sub_gauge_topic = oplus_mms_get_by_name("gauge:1");
	return !!chip->sub_gauge_topic;
}

#define USB_54C 54
#define USB_57C 57
#define USB_48C 48
#define USB_12C 12
#define USB_6C 6
#define USB_MAX 100
static void oplus_usbtemp_high_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip =
		container_of(dwork, struct oplus_chg_reverse, usbtemp_high_work);
	int rc;
	union mms_msg_data data = { 0 };
	int usb_temp_l;
	int usb_temp_r;
	int usbtemp_volt_l;
	int usbtemp_volt_r;

	oplus_mms_get_item_data(chip->wired_topic, WIRED_ITEM_USB_STATUS, &data, false);

	oplus_wired_get_usb_temp_volt(&usbtemp_volt_l, &usbtemp_volt_r);
	oplus_wired_get_usb_temp(&usb_temp_l, &usb_temp_r);
	if ((usb_temp_l >= USB_54C || usb_temp_r >= USB_54C
		|| usb_temp_l - chip->batt_realy_temp / 10 >= USB_12C
		|| usb_temp_r - chip->batt_realy_temp / 10 >= USB_12C)
		&& (chip->high_reverse_enable)) {
		vote(chip->reverse_pdo_votable, USB_VOTER, true,
				(PD_VBUS << 16 | NORMAL_IBUS), false);
	} else if (usb_temp_l <= USB_48C && usb_temp_r <= USB_48C
		&& usb_temp_l - chip->batt_realy_temp / 10 <= USB_6C
		&& usb_temp_r - chip->batt_realy_temp / 10 <= USB_6C) {
		vote(chip->reverse_pdo_votable, USB_VOTER, false, 0, false);
	}
	if ((data.intval & USB_TEMP_HIGH) == USB_TEMP_HIGH) {
		rc = oplus_chg_ic_func(g_rvs_chg_chip->reverse_ic,
					OPLUS_IC_FUNC_RVS_SET_HIGH_PWR_MODE_EN, 0);
		vote(chip->reverse_pdo_votable, USB_VOTER, true,
				(NORMAL_VBUS << 16 | NORMAL_IBUS), false);
		vote(chip->high_reverse_disable_votable, USB_VOTER, true, 1, false);
		msleep(WAIT_HIGH_REVERSE_COLSE);
		oplus_wired_set_dischg_status(true);
	}
}

static void oplus_reverse_wired_subs_callback(struct mms_subscribe *subs,
					  enum mms_msg_type type, u32 id, bool sync)
{
	struct oplus_chg_reverse *chip = subs->priv_data;
	union mms_msg_data data = { 0 };

	switch (type) {
	case MSG_TYPE_TIMER:
		break;
	case MSG_TYPE_ITEM:
		switch (id) {
		case WIRED_ITEM_USB_STATUS:
			oplus_mms_get_item_data(chip->wired_topic, id, &data, false);
			if (((data.intval & USB_TEMP_HIGH) == USB_TEMP_HIGH) &&
				chip->reverse_enable) {
				schedule_delayed_work(&chip->usbtemp_high_work, 0);
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static void oplus_reverse_subscribe_wired_topic(struct oplus_mms *topic,
					     void *prv_data)
{
	struct oplus_chg_reverse *chip = prv_data;

	chip->wired_topic = topic;
	chip->wired_subs =
		oplus_mms_subscribe(chip->wired_topic, chip,
				    oplus_reverse_wired_subs_callback, "reverse");
	if (IS_ERR_OR_NULL(chip->wired_subs)) {
		return;
	}
}

static void oplus_reverse_gauge_subs_callback(struct mms_subscribe *subs,
					   enum mms_msg_type type, u32 id, bool sync)
{
	struct oplus_chg_reverse *chip = subs->priv_data;
	union mms_msg_data data = { 0 };
	int rc;

	switch (type) {
	case MSG_TYPE_TIMER:
		oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_VOL_MAX,
					&data, false);
		chip->vbat_mv = data.intval;
		oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_TEMP,
					&data, false);
		chip->batt_temp = data.intval;
		oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_CURR,
					&data, false);
		chip->ibat_ma = data.intval;
		if (is_sub_gauge_topic_available(chip)) {
			oplus_mms_get_item_data(chip->sub_gauge_topic, GAUGE_ITEM_VOL_MAX,
						&data, false);
			chip->vbat_min_mv = data.intval;
		} else {
			oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_VOL_MIN,
						&data, false);
			chip->vbat_min_mv = data.intval;
		}
		oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_SOC,
					&data, false);
		chip->batt_soc = data.intval;
		oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_REAL_TEMP,
				&data, false);
		chip->batt_realy_temp = data.intval;
		break;
	case MSG_TYPE_ITEM:
		switch (id) {
		case GAUGE_ITEM_AUTH:
			rc = oplus_mms_get_item_data(chip->gauge_topic, id,
				&data, false);
			if (rc < 0) {
			} else {
				chip->authenticate = !!data.intval;
			}
			break;
		case GAUGE_ITEM_HMAC:
			rc = oplus_mms_get_item_data(chip->gauge_topic, id,
								&data, false);
			if (rc < 0) {
			} else {
				chip->hmac = !!data.intval;
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static void oplus_reverse_subscribe_gauge_topic(struct oplus_mms *topic,
						  void *prv_data)
{
	struct oplus_chg_reverse *chip = prv_data;
	union mms_msg_data data = { 0 };
	int rc;

	chip->gauge_topic = topic;
	chip->gauge_subs =
		oplus_mms_subscribe(chip->gauge_topic, chip,
					oplus_reverse_gauge_subs_callback, "reverse");
	if (IS_ERR_OR_NULL(chip->gauge_subs)) {
		return;
	}
	oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_VOL_MAX, &data,
				true);
	chip->vbat_mv = data.intval;
	oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_TEMP, &data,
				true);
	chip->batt_temp = data.intval;
	if (is_sub_gauge_topic_available(chip)) {
		oplus_mms_get_item_data(chip->sub_gauge_topic, GAUGE_ITEM_VOL_MAX,
					&data, false);
		chip->vbat_min_mv = data.intval;
	} else {
		oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_VOL_MIN, &data,
					true);
		chip->vbat_min_mv = data.intval;
	}
	oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_SOC, &data,
				true);
	chip->batt_soc = data.intval;

	rc = oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_HMAC, &data,
				     true);
	if (rc < 0) {
		chip->hmac = false;
	} else {
		chip->hmac = !!data.intval;
	}
	rc = oplus_mms_get_item_data(chip->gauge_topic, GAUGE_ITEM_AUTH, &data,
				true);
	if (rc < 0) {
		chip->authenticate = false;
	} else {
		chip->authenticate = !!data.intval;
	}

}

static int oplus_reverse_topic_init(struct oplus_chg_reverse *chip)
{
	struct oplus_mms_config mms_cfg = {};
	int rc;

	mms_cfg.drv_data = chip;
	mms_cfg.of_node = chip->dev->of_node;

	chip->reverse_topic =
		devm_oplus_mms_register(chip->dev, &oplus_reverse_desc, &mms_cfg);
	if (IS_ERR(chip->reverse_topic)) {
		rc = PTR_ERR(chip->reverse_topic);
		return rc;
	}

	oplus_mms_wait_topic("wired", oplus_reverse_subscribe_wired_topic, chip);
	oplus_mms_wait_topic("common", oplus_reverse_subscribe_comm_topic, chip);
	oplus_mms_wait_topic("gauge", oplus_reverse_subscribe_gauge_topic, chip);
	return 0;
}

static int oplus_high_reverse_disable_vote_callback(struct votable *votable, void *data,
					    int disable, const char *client,
					    bool step)
{
	struct oplus_chg_reverse *chip = data;
	static bool high_reverse_disable;

	if (disable < 0)
		high_reverse_disable = false;
	else
		high_reverse_disable = !!disable;
	if (high_reverse_disable && chip->req_volt == PD_VBUS)
		schedule_delayed_work(&chip->reverse_error_flag_work, 0);

	return 0;
}

static int oplus_reverse_pdo_vote_callback(struct votable *votable, void *data,
					 int pdo, const char *client,
					 bool step)
{
	struct oplus_chg_reverse *chip = data;

	if (pdo < 0)
		return -EINVAL;

	chip->target_pdo = pdo;

	return 0;
}

static int oplus_reverse_cap_pdo0_vote_callback(struct votable *votable, void *data,
					 int pdo, const char *client,
					 bool step)
{
	struct oplus_chg_reverse *chip = data;

	if (pdo < 0)
		return -EINVAL;

	chip->target_cap_pdo0 = pdo;

	return 0;
}

static int oplus_reverse_vote_init(struct oplus_chg_reverse *chip)
{
	int rc;

	chip->reverse_pdo_votable = create_votable("REVERSE_PDO", VOTE_MIN,
					   oplus_reverse_pdo_vote_callback, chip);
	if (IS_ERR(chip->reverse_pdo_votable)) {
		rc = PTR_ERR(chip->reverse_pdo_votable);
		chip->reverse_pdo_votable = NULL;
		return rc;
	}

	chip->high_reverse_disable_votable =
		create_votable("HIGH_REVERSE_DISABLE", VOTE_SET_ANY, oplus_high_reverse_disable_vote_callback, chip);
	if (IS_ERR(chip->high_reverse_disable_votable)) {
		rc = PTR_ERR(chip->high_reverse_disable_votable);
		chip->high_reverse_disable_votable = NULL;
		goto creat_disable_votable_err;
	}

	chip->cap_pdo0_votable = create_votable("REVERSE_CAP_PDO0", VOTE_MIN,
					   oplus_reverse_cap_pdo0_vote_callback, chip);
	if (IS_ERR(chip->cap_pdo0_votable)) {
		rc = PTR_ERR(chip->cap_pdo0_votable);
		chip->cap_pdo0_votable = NULL;
		goto creat_cap_pdo0_votable_err;
	}

	vote(chip->high_reverse_disable_votable, BATT_TEMP_VOTER, false, 0, false);
	vote(chip->high_reverse_disable_votable, BATT_SOC_VOTER, false, 0, false);
	return 0;
creat_cap_pdo0_votable_err:
	destroy_votable(chip->high_reverse_disable_votable);
creat_disable_votable_err:
	destroy_votable(chip->reverse_pdo_votable);
	return rc;
}

static void oplus_reverse_buck_ic_reg_callback(struct oplus_chg_ic_dev *ic, void *data, bool timeout)
{
	struct oplus_chg_reverse *chip;

	if (data == NULL) {
		return;
	}
	chip = data;

	chip->buck_ic = ic;
}

static void oplus_reverse_cp_ic_reg_callback(struct oplus_chg_ic_dev *ic, void *data, bool timeout)
{
	struct oplus_chg_reverse *chip;
	const char *name;

	if (data == NULL) {
		return;
	}
	chip = data;

	chip->cp_ic = ic;

	name = of_get_oplus_chg_ic_name(chip->dev->of_node, "oplus,buck_ic", 0);
	oplus_chg_ic_wait_ic(name, oplus_reverse_buck_ic_reg_callback, chip);
}

static int oplus_reverse_chg_virq_register(struct oplus_chg_reverse *chip)
{
	int rc;

	rc = oplus_chg_ic_virq_register(chip->reverse_ic, OPLUS_IC_VIRQ_ONLINE,
		oplus_reverse_chg_online_handler, chip);
	rc = oplus_chg_ic_virq_register(chip->reverse_ic, OPLUS_IC_VIRQ_OFFLINE,
		oplus_high_reverse_chg_offline_handler, chip);
	rc = oplus_chg_ic_virq_register(chip->reverse_ic, OPLUS_IC_VIRQ_ERR,
		oplus_high_reverse_chg_err_handler, chip);
	rc = oplus_chg_ic_virq_register(chip->reverse_ic, OPLUS_IC_VIRQ_REVERSE_ENABLE,
		oplus_reverse_chg_enable_handler, chip);
	rc = oplus_chg_ic_virq_register(chip->reverse_ic, OPLUS_IC_VIRQ_HIGH_REVERSE_ENABLE,
		oplus_high_reverse_chg_enable_handler, chip);
	rc = oplus_chg_ic_virq_register(chip->reverse_ic, OPLUS_IC_VIRQ_HARD_RESET,
		oplus_reverse_hard_reset_handler, chip);
	rc = oplus_chg_ic_virq_register(chip->reverse_ic, OPLUS_IC_VIRQ_SINK_REQ_MSG,
		oplus_reverse_sink_req_msg_handler, chip);
	return rc;
}

static void oplus_reverse_reverse_ic_reg_callback(struct oplus_chg_ic_dev *ic, void *data, bool timeout)
{
	struct oplus_chg_reverse *chip;
	const char *name;
	int rc;

	if (data == NULL) {
		return;
	}
	chip = data;

	chip->reverse_ic = ic;

	rc = oplus_reverse_chg_virq_register(chip);
	if (rc < 0) {
		return;
	}

	name = of_get_oplus_chg_ic_name(chip->dev->of_node, "oplus,cp_ic", 0);
	oplus_chg_ic_wait_ic(name, oplus_reverse_cp_ic_reg_callback, chip);
}

static void oplus_reverse_chg_init_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct oplus_chg_reverse *chip = container_of(dwork,
		struct oplus_chg_reverse, reverse_chg_init_work);
	struct device_node *node = chip->dev->of_node;
	const char *name;

	name = of_get_oplus_chg_ic_name(node, "oplus,reverse_ic", 0);
	oplus_chg_ic_wait_ic(name, oplus_reverse_reverse_ic_reg_callback, chip);
}

static int oplus_chg_reverse_probe(struct platform_device *pdev)
{
	struct oplus_chg_reverse *chip;
	int rc;

	chip = devm_kzalloc(&pdev->dev, sizeof(struct oplus_chg_reverse), GFP_KERNEL);
	if (chip == NULL) {
		return -ENOMEM;
	}
	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);
	rc = oplus_reverse_topic_init(chip);
	if (rc < 0)
		goto topic_reg_err;
	g_rvs_chg_chip = chip;

	rc = oplus_reverse_parse_dt(chip);
	if (rc < 0)
		goto parse_dt_err;
	oplus_reverse_awake_init(chip);
	chip->vbus_level = chip->vbus_limit_count;
	chip->vbat_level = chip->vbat_limit_count;
	INIT_DELAYED_WORK(&chip->reverse_chg_init_work,
			  oplus_reverse_chg_init_work);
	INIT_DELAYED_WORK(&chip->reverse_monitor_work, oplus_reverse_monitor_work);
	INIT_DELAYED_WORK(&chip->usbtemp_high_work, oplus_usbtemp_high_work);
	INIT_DELAYED_WORK(&chip->reverse_pdo_update_work, oplus_reverse_pdo_update_work);
	INIT_DELAYED_WORK(&chip->reverse_chg_type_work,
			  oplus_reverse_chg_type_work);
	INIT_DELAYED_WORK(&chip->reverse_error_flag_work,
	oplus_reverse_error_flag_work);
	INIT_DELAYED_WORK(&chip->normal_reverse_set_hw_ocp_work,
	oplus_normal_reverse_set_hw_ocp_work);
	INIT_DELAYED_WORK(&chip->reverse_vbus_retention_check_work,
	oplus_reverse_vbus_retention_check_work);
	INIT_DELAYED_WORK(&chip->reverse_chg_test_mode_work,
			  oplus_reverse_chg_test_mode_work);
	INIT_DELAYED_WORK(&chip->reverse_clear_keep_status_work,
			  oplus_reverse_clear_keep_status_work);
	INIT_DELAYED_WORK(&chip->reverse_hard_reset_work,
			  oplus_reverse_hard_reset_work);
	INIT_WORK(&chip->reverse_online_work, oplus_reverse_online_work);
	INIT_WORK(&chip->reverse_vbus_check_work,
			  oplus_reverse_vbus_check_work);
	mutex_init(&chip->set_pdo_lock);
	oplus_reverse_vote_init(chip);
	schedule_delayed_work(&chip->reverse_chg_init_work, 0);
	oplus_chg_reverse_watchdog_init(chip);
	return 0;

topic_reg_err:
parse_dt_err:
	devm_kfree(&pdev->dev, chip);
	g_rvs_chg_chip = NULL;
	return rc;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0))
static void oplus_chg_reverse_remove(struct platform_device *pdev)
#else
static int oplus_chg_reverse_remove(struct platform_device *pdev)
#endif
{
	struct oplus_chg_reverse *chip = platform_get_drvdata(pdev);

	if (!IS_ERR_OR_NULL(chip->comm_subs))
		oplus_mms_unsubscribe(chip->comm_subs);
	if (!IS_ERR_OR_NULL(chip->wired_subs))
		oplus_mms_unsubscribe(chip->wired_subs);
	if (!IS_ERR_OR_NULL(chip->reverse_subs))
		oplus_mms_unsubscribe(chip->reverse_subs);
	if (!IS_ERR_OR_NULL(chip->gauge_subs))
		oplus_mms_unsubscribe(chip->gauge_subs);
	if (chip->reverse_pdo_votable != NULL)
		destroy_votable(chip->reverse_pdo_votable);
	if (chip->high_reverse_disable_votable != NULL)
		destroy_votable(chip->high_reverse_disable_votable);
	oplus_reverse_awake_exit(chip);
	kthread_stop(chip->reverse_watchdog);
	devm_kfree(&pdev->dev, chip);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0))
	return 0;
#endif
}

static const struct of_device_id oplus_chg_reverse_match[] = {
	{.compatible = "oplus,reverse_charge"},
	{},
};

static struct platform_driver oplus_chg_reverse_driver = {
	.driver = {
		.name = "oplus-reverse_charge",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(oplus_chg_reverse_match),
	},
	.probe = oplus_chg_reverse_probe,
	.remove = oplus_chg_reverse_remove,
};

static __init int oplus_chg_reverse_init(void)
{
	return platform_driver_register(&oplus_chg_reverse_driver);
}

static __exit void oplus_chg_reverse_exit(void)
{
	platform_driver_unregister(&oplus_chg_reverse_driver);
}

oplus_chg_module_register(oplus_chg_reverse);

int oplus_set_reverse_chg_type(struct oplus_mms *topic, int type)
{
	struct oplus_chg_reverse *chip;

	if (topic == NULL) {
		return -1;
	}

	chip = oplus_mms_get_drvdata(topic);
	if (chip == NULL) {
		return -1;
	}

	switch (type) {
	case REVERSE_CHG_TYPE_UFCS:
		oplus_set_reverse_chg_type_pd(chip, false);
		oplus_set_reverse_chg_type_pps(chip, false);
		oplus_set_reverse_chg_type_ufcs(chip, true);
		break;
	case REVERSE_CHG_TYPE_PPS:
		oplus_set_reverse_chg_type_pd(chip, false);
		oplus_set_reverse_chg_type_ufcs(chip, false);
		oplus_set_reverse_chg_type_pps(chip, true);
		break;
	case REVERSE_CHG_TYPE_PD:
		oplus_set_reverse_chg_type_ufcs(chip, false);
		oplus_set_reverse_chg_type_pps(chip, false);
		oplus_set_reverse_chg_type_pd(chip, true);
		break;
	case REVERSE_CHG_TYPE_NORMAL:
		oplus_set_reverse_chg_type_ufcs(chip, false);
		oplus_set_reverse_chg_type_pps(chip, false);
		oplus_set_reverse_chg_type_pd(chip, false);
		break;
	default:
		;
		break;
	}
	return 0;
}
