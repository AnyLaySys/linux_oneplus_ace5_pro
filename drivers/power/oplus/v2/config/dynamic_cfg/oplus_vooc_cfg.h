// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022-2023 Oplus. All rights reserved.
 */

#define LOG_BUF_SIZE	4096
static char *g_log_buf;

static void oplus_vooc_update_temp_region_config(
	struct oplus_param_head *param_head, struct oplus_vooc_spec_config *spec)
{
	struct oplus_cfg_data_head *data_head;
	int32_t buf[1];
	int rc;

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_low_temp");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_low_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_little_cold_temp ");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_little_cold_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_cool_temp  ");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_cool_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_little_cool_temp");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_little_cool_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_little_cool_high_temp");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_little_cool_high_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_normal_low_temp");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_normal_low_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_normal_high_temp");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_normal_high_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_high_temp");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_high_temp = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_over_high_temp");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_over_high_temp = le32_to_cpu(buf[0]);
		break;
	}
}

static void oplus_vooc_update_soc_region_config(
	struct oplus_param_head *param_head, struct oplus_vooc_spec_config *spec)
{
	struct oplus_cfg_data_head *data_head;
	int32_t buf[VOOC_SOC_RANGE_NUM];
	int i, rc;
	int index;

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_low_soc");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_low_soc = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_high_soc  ");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		spec->vooc_high_soc  = le32_to_cpu(buf[0]);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_soc_range");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		index = 0;
		for (i = 0; i < VOOC_SOC_RANGE_NUM; i++) {
			spec->vooc_soc_range[i] = le32_to_cpu(buf[i]);
			if (g_log_buf) {
				index += snprintf(g_log_buf + index, LOG_BUF_SIZE - index - 1, "%s%d",
					(i == 0) ? "" : ", ", spec->vooc_soc_range[i]);
				g_log_buf[index] = 0;
			}
		}
		break;
	}
}

static void oplus_vooc_update_spec_misc_config(
	struct oplus_param_head *param_head, struct oplus_vooc_spec_config *spec)
{
	struct oplus_cfg_data_head *data_head;
	int32_t buf;
	int rc;

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_warm_vol_thr");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		spec->vooc_warm_vol_thr = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_warm_soc_thr");
	while (data_head == NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		spec->vooc_warm_soc_thr = le32_to_cpu(buf);
		break;
	}
}

static void oplus_vooc_update_mcu_bad_volt_config(
	struct oplus_param_head *param_head, struct oplus_vooc_spec_config *spec)
{
	struct oplus_cfg_data_head *data_head;
	int32_t buf[VOOC_BAT_VOLT_REGION];
	int i, rc;
	int index;

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_bad_volt");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		index = 0;
		for (i = 0; i < VOOC_BAT_VOLT_REGION; i++) {
			spec->vooc_bad_volt[i] = le32_to_cpu(buf[i]);
			if (g_log_buf) {
				index += snprintf(g_log_buf + index, LOG_BUF_SIZE - index - 1, "%s%d",
					(i == 0) ? "" : ", ", spec->vooc_bad_volt[i]);
				g_log_buf[index] = 0;
			}
		}
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus_spec,vooc_bad_volt_suspend");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)buf, sizeof(buf[0]));
		if (rc < 0) {
			break;
		}
		index = 0;
		for (i = 0; i < VOOC_BAT_VOLT_REGION; i++) {
			spec->vooc_bad_volt_suspend[i] = le32_to_cpu(buf[i]);
			if (g_log_buf) {
				index += snprintf(g_log_buf + index, LOG_BUF_SIZE - index - 1, "%s%d",
					(i == 0) ? "" : ", ", spec->vooc_bad_volt_suspend[i]);
				g_log_buf[index] = 0;
			}
		}
		break;
	}
}

struct batt_bcc_curves *g_bcc_curves_metadata[] = {
	bcc_curves_soc0_2_50,
	bcc_curves_soc50_2_75,
	bcc_curves_soc75_2_85,
	bcc_curves_soc85_2_90
};

#define NAME_BUF_MAX 256
static int oplus_get_strategy_data(
	struct oplus_param_head *param_head, const char *name, struct batt_bcc_curves *metadata[])
{
	struct oplus_cfg_data_head *data_head;
	ssize_t data_len;
	const int range_data_size = sizeof(struct batt_bcc_curve);
	char *tmp_buf;
	char name_buf[NAME_BUF_MAX];
	int curv_num[BCC_BATT_SOC_90_TO_100][BATT_BCC_CURVE_MAX] = { 0 };
	bool skip[BCC_BATT_SOC_90_TO_100][BATT_BCC_CURVE_MAX] = { 0 };
	int i, j;
	int rc = 0;
	int m;
	struct batt_bcc_curve *tmp;

	tmp_buf = kzalloc(BCC_BATT_SOC_90_TO_100 * BATT_BCC_CURVE_MAX * BATT_BCC_ROW_MAX * range_data_size, GFP_KERNEL);
	if (!tmp_buf) {
		return -ENOMEM;
	}

	for (i = 0; i < BCC_BATT_SOC_90_TO_100; i++) {
		for (j = 0; j < BATT_BCC_CURVE_MAX; j++) {
			memset(name_buf, 0, NAME_BUF_MAX);
			snprintf(name_buf, NAME_BUF_MAX, "%s:%s:%s", name, strategy_soc[i], strategy_temp[j]);
			data_head = oplus_cfg_find_param_by_name(param_head, name_buf);
			if (data_head == NULL) {
				rc = -ENODATA;
				skip[i][j] = true;
				continue;
			}
			data_len = oplus_cfg_get_data_size(data_head);
			if (data_len < 0 || data_len % range_data_size) {
				rc = -EINVAL;
				goto err;
			}
			rc = oplus_cfg_get_data(
				data_head,
				tmp_buf + (BATT_BCC_CURVE_MAX * i + j) * BATT_BCC_ROW_MAX * range_data_size,
				data_len);
			if (rc < 0) {
				goto err;
			}
			curv_num[i][j] = data_len / range_data_size;
		}
	}

	for (i = 0; i < BCC_BATT_SOC_90_TO_100; i++) {
		for (j = 0; j < BATT_BCC_CURVE_MAX; j++) {
			if (skip[i][j])
				continue;
			memcpy(metadata[i][j].batt_bcc_curve,
			       tmp_buf + (BATT_BCC_CURVE_MAX * i + j) * BATT_BCC_ROW_MAX * range_data_size,
			       BATT_BCC_ROW_MAX * range_data_size);
			metadata[i][j].bcc_curv_num = curv_num[i][j];

			tmp = metadata[i][j].batt_bcc_curve;
			for (m = 0; m < curv_num[i][j]; m++)
				;
		}
	}

err:
	kfree(tmp_buf);
	return rc;
}

static void oplus_vooc_update_bcc_curve_config(
	struct oplus_param_head *param_head, struct oplus_vooc_spec_config *spec)
{
	int rc;

	rc = oplus_get_strategy_data(param_head, "oplus_spec,svooc_charge_curve", g_bcc_curves_metadata);
}

static int oplus_vooc_update_spec_config(void *data, struct oplus_param_head *param_head)
{
	struct oplus_vooc_spec_config *spec;

	if (data == NULL) {
		return -EINVAL;
	}
	if (param_head == NULL) {
		return -EINVAL;
	}
	spec = (struct oplus_vooc_spec_config *)data;

	g_log_buf = kzalloc(LOG_BUF_SIZE, GFP_KERNEL);
	oplus_vooc_update_temp_region_config(param_head, spec);
	oplus_vooc_update_soc_region_config(param_head, spec);
	oplus_vooc_update_spec_misc_config(param_head, spec);
	oplus_vooc_update_mcu_bad_volt_config(param_head, spec);
	oplus_vooc_update_bcc_curve_config(param_head, spec);

	if (g_log_buf != NULL) {
		kfree(g_log_buf);
		g_log_buf = NULL;
	}

	return 0;
}

static void oplus_vooc_update_misc_config(
	struct oplus_param_head *param_head, struct oplus_vooc_config *config)
{
	struct oplus_chg_vooc *chip;
	struct oplus_cfg_data_head *data_head;
	u8 enable;
	uint32_t buf;
	int rc;

	chip = container_of(config, struct oplus_chg_vooc, config);

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,vooc-version");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		config->vooc_version = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,vooc_data_width");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		config->data_width = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,vooc_curr_max");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		config->max_curr_level = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,vooc_project");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		config->vooc_project = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,vooc_power_max_w");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		config->max_power_w = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,voocphy_support");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		config->voocphy_support = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,vooc_curr_table_type");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&buf, sizeof(buf));
		if (rc < 0) {
			break;
		}
		config->vooc_curr_table_type = le32_to_cpu(buf);
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,support_vooc_by_normal_charger_path");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&enable, sizeof(enable));
		if (rc < 0) {
			break;
		}
		config->support_vooc_by_normal_charger_path = !!enable;
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,vooc_bad_volt_check_support");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&enable, sizeof(enable));
		if (rc < 0) {
			break;
		}
		config->vooc_bad_volt_check_support = !!enable;
		break;
	}

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,smart_chg_bcc_support");
	while (data_head != NULL) {
		rc = oplus_cfg_get_data(data_head, (u8 *)&enable, sizeof(enable));
		if (rc < 0) {
			break;
		}
		chip->smart_chg_bcc_support = !!enable;
		break;
	}
}

#define STRATEGY_NAME_LEN	128
static void oplus_vooc_update_strategy_config(
	struct oplus_param_head *param_head, struct oplus_vooc_config *config)
{
	struct oplus_chg_vooc *chip;
	struct oplus_cfg_data_head *data_head;
	struct oplus_chg_strategy *vooc_strategy;
	char name[STRATEGY_NAME_LEN] = { 0 };
	u8 *data_buf;
	ssize_t data_len;
	int i, index;
	int rc;

	chip = container_of(config, struct oplus_chg_vooc, config);

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,general_strategy_name");
	if (data_head == NULL)
		return;
	data_len = oplus_cfg_get_data_size(data_head);
	if (data_len >= STRATEGY_NAME_LEN) {
		return;
	}
	rc = oplus_cfg_get_data(data_head, name, data_len);
	if (rc < 0) {
		return;
	}
	name[data_len] = 0;

	data_head = oplus_cfg_find_param_by_name(param_head, "oplus,general_strategy_data");
	if (data_head == NULL) {
		return;
	}
	data_len = oplus_cfg_get_data_size(data_head);
	data_buf = devm_kzalloc(chip->dev, data_len, GFP_KERNEL);
	if (data_buf == NULL) {
		return;
	}

	rc = oplus_cfg_get_data(data_head, data_buf, data_len);
	if (rc < 0) {
		goto out;
	}
	vooc_strategy = oplus_chg_strategy_alloc(name, data_buf, data_len);
	if (vooc_strategy == NULL)
		;
	else
		chip->general_strategy = vooc_strategy;

	if (g_log_buf) {
		index = 0;
		for (i = 0; i < (data_len / sizeof(u32)); i++) {
			index += snprintf(g_log_buf + index, LOG_BUF_SIZE - index - 1, "%s%d",
				(i == 0) ? "" : ", ", le32_to_cpu(*((u32 *)(data_buf + i * sizeof(u32)))));
			g_log_buf[index] = 0;
		}
	}

out:
	devm_kfree(chip->dev, data_buf);
}

static int oplus_vooc_update_normal_config(void *data, struct oplus_param_head *param_head)
{
	struct oplus_vooc_config *config;

	if (data == NULL) {
		return -EINVAL;
	}
	if (param_head == NULL) {
		return -EINVAL;
	}
	config = (struct oplus_vooc_config *)data;

	g_log_buf = kzalloc(LOG_BUF_SIZE, GFP_KERNEL);
	oplus_vooc_update_misc_config(param_head, config);
	oplus_vooc_update_strategy_config(param_head, config);

	if (g_log_buf != NULL) {
		kfree(g_log_buf);
		g_log_buf = NULL;
	}

	return 0;
}

static int oplus_vooc_reg_debug_config(struct oplus_chg_vooc *chip)
{
	int rc;

	chip->spec_debug_cfg.type = OPLUS_CHG_VOOC_SPEC_PARAM;
	chip->spec_debug_cfg.update = oplus_vooc_update_spec_config;
	chip->spec_debug_cfg.priv_data = &chip->spec;
	rc = oplus_cfg_register(&chip->spec_debug_cfg);

	chip->normal_debug_cfg.type = OPLUS_CHG_VOOC_NORMAL_PARAM;
	chip->normal_debug_cfg.update = oplus_vooc_update_normal_config;
	chip->normal_debug_cfg.priv_data = &chip->config;
	rc = oplus_cfg_register(&chip->normal_debug_cfg);

	return 0;
}

static void oplus_vooc_unreg_debug_config(struct oplus_chg_vooc *chip)
{
	oplus_cfg_unregister(&chip->spec_debug_cfg);
	oplus_cfg_unregister(&chip->normal_debug_cfg);
}
