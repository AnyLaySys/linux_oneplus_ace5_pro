// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "[STRATEGY_INR]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <oplus_chg.h>
#include <oplus_mms.h>
#include <oplus_mms_gauge.h>
#include <oplus_chg_comm.h>
#include <oplus_strategy.h>

#define INR_DATA_MIN_SIZE 3

struct inr_strategy_region {
	int index;
	int ref_data;
};

struct inr_strategy {
	struct oplus_chg_strategy strategy;
	int32_t *default_data;
	int32_t *dynamic_data;
	int data_num;
	int anti_thr;
	int ref_type;
	struct inr_strategy_region region;
};

static struct oplus_mms *comm_topic;
static struct oplus_mms *gauge_topic;
static struct oplus_mms *main_gauge_topic;
static struct oplus_mms *sub_gauge_topic;

__maybe_unused static bool is_comm_topic_available(void)
{
	if (!comm_topic)
		comm_topic = oplus_mms_get_by_name("common");
	return !!comm_topic;
}

__maybe_unused static bool is_gauge_topic_available(void)
{
	if (!gauge_topic)
		gauge_topic = oplus_mms_get_by_name("gauge");
	return !!gauge_topic;
}

__maybe_unused static bool is_main_gauge_topic_available(void)
{
	if (!main_gauge_topic)
		main_gauge_topic = oplus_mms_get_by_name("gauge:0");
	return !!main_gauge_topic;
}

__maybe_unused static bool is_sub_gauge_topic_available(void)
{
	if (!sub_gauge_topic)
		sub_gauge_topic = oplus_mms_get_by_name("gauge:1");
	return !!sub_gauge_topic;
}

static int inr_strategy_get_index(struct inr_strategy *strategy, int temp)
{
	int i;

	for (i = 0; i < strategy->data_num; i++) {
		if (temp <= strategy->dynamic_data[i])
			return i;
	}

	return strategy->data_num;
}

static int inr_strategy_get_ref_data(struct inr_strategy *inr, int *ref_data)
{
	union mms_msg_data data = { 0 };
	int rc;

	switch (inr->ref_type) {
	case STRATEGY_USE_BATT_TEMP:
		if (!is_gauge_topic_available()) {
			return -ENODEV;
		}
		rc = oplus_mms_get_item_data(gauge_topic, GAUGE_ITEM_TEMP,
					     &data, false);
		if (rc < 0) {
			return rc;
		}

		*ref_data = data.intval;
		break;
	case STRATEGY_USE_SHELL_TEMP:
		if (!is_comm_topic_available()) {
			return -ENODEV;
		}
		rc = oplus_mms_get_item_data(comm_topic, COMM_ITEM_SHELL_TEMP,
					     &data, false);
		if (rc < 0) {
			return rc;
		}

		*ref_data = data.intval;
		break;
	case STRATEGY_USE_MAIN_BATT_TEMP:
		if (!is_main_gauge_topic_available()) {
			return -ENODEV;
		}
		rc = oplus_mms_get_item_data(main_gauge_topic, GAUGE_ITEM_TEMP,
					     &data, false);
		if (rc < 0) {
			return rc;
		}

		*ref_data = data.intval;
		break;
	case STRATEGY_USE_SUB_BATT_TEMP:
		if (!is_sub_gauge_topic_available()) {
			return -ENODEV;
		}
		rc = oplus_mms_get_item_data(sub_gauge_topic, GAUGE_ITEM_TEMP,
					     &data, false);
		if (rc < 0) {
			return rc;
		}

		*ref_data = data.intval;
		break;
	default:
		;
		return -EINVAL;
	}

	return 0;
}

static struct oplus_chg_strategy *
inr_strategy_alloc(unsigned char *buf, size_t size)
{
	struct inr_strategy *strategy;
	int data_num;
	int data_size;

	if (buf == NULL) {
		return ERR_PTR(-EINVAL);
	}
	if (size < INR_DATA_MIN_SIZE * sizeof(u32) || (size % sizeof(u32))) {
		return ERR_PTR(-EINVAL);
	}

	data_size = size - sizeof(u32) - sizeof(u32);
	data_num =  data_size/ sizeof(u32);

	strategy = kzalloc(sizeof(struct inr_strategy), GFP_KERNEL);
	if (strategy == NULL) {
		return ERR_PTR(-ENOMEM);
	}
	strategy->ref_type = *((u32 *)buf);
	if ((strategy->ref_type != STRATEGY_USE_BATT_TEMP) &&
	    (strategy->ref_type != STRATEGY_USE_SHELL_TEMP) &&
	    (strategy->ref_type != STRATEGY_USE_MAIN_BATT_TEMP) &&
	    (strategy->ref_type != STRATEGY_USE_SUB_BATT_TEMP)) {
		kfree(strategy);
		return ERR_PTR(-EINVAL);
	}

	strategy->anti_thr = *((u32 *)(buf + sizeof(u32)));

	strategy->data_num = data_num;
	strategy->default_data = kzalloc(data_size, GFP_KERNEL);
	if (strategy->default_data == NULL) {
		kfree(strategy);
		return ERR_PTR(-ENOMEM);
	}

	strategy->dynamic_data = kzalloc(data_size, GFP_KERNEL);
	if (strategy->dynamic_data == NULL) {
		kfree(strategy->default_data);
		kfree(strategy);
		return ERR_PTR(-ENOMEM);
	}

	memcpy(strategy->default_data, buf + sizeof(u32) + sizeof(u32), data_size);
	memcpy(strategy->dynamic_data, buf + sizeof(u32) + sizeof(u32), data_size);

	return &strategy->strategy;
}

static int inr_strategy_release(struct oplus_chg_strategy *strategy)
{
	struct inr_strategy *inr;

	if (strategy == NULL) {
		return -EINVAL;
	}
	inr = (struct inr_strategy *)strategy;

	kfree(inr->dynamic_data);
	kfree(inr->default_data);
	kfree(inr);

	return 0;
}

static int inr_strategy_init(struct oplus_chg_strategy *strategy)
{
	struct inr_strategy *inr;
	int ref_data;
	int rc;

	if (strategy == NULL) {
		return -EINVAL;
	}

	inr = (struct inr_strategy *)strategy;
	rc = inr_strategy_get_ref_data(inr, &ref_data);
	if (rc < 0) {
		return rc;
	}

	inr->region.ref_data = ref_data;
	inr->region.index = inr_strategy_get_index(inr, ref_data);

	return 0;
}

static void inr_strategy_ref_region_thr_update(
	struct inr_strategy *strategy, int pre_index, int curr_index)
{
	int i;

	for (i = 0; i < strategy->data_num; i++)
		strategy->dynamic_data[i] = strategy->default_data[i];

	if ((pre_index > curr_index) &&
	    (pre_index - curr_index == 1) &&
	    (curr_index < strategy->data_num)) {
		strategy->dynamic_data[curr_index] =
		strategy->default_data[curr_index] + strategy->anti_thr;
	} else if ((pre_index < curr_index) &&
		   (curr_index - pre_index == 1) &&
		   (pre_index < strategy->data_num)) {
		strategy->dynamic_data[pre_index] =
		strategy->default_data[pre_index] - strategy->anti_thr;
	}

	for (i = 0; i< strategy->data_num; i++)
		;

}

static int inr_strategy_get_data(struct oplus_chg_strategy *strategy, void *ret)
{
	struct inr_strategy *inr;
	int ref_data;
	int rc;
	int index;

	if (strategy == NULL) {
		return -EINVAL;
	}
	if (ret == NULL) {
		return -EINVAL;
	}

	inr = (struct inr_strategy *)strategy;
	rc = inr_strategy_get_ref_data(inr, &ref_data);
	if (rc < 0) {
		return rc;
	}

	index = inr_strategy_get_index(inr, ref_data);
	if (inr->region.index != index)
		inr_strategy_ref_region_thr_update(inr, inr->region.index, index);

	inr->region.index = index;
	*((int *)ret) = index;

	return 0;
}

static struct oplus_chg_strategy_desc inr_strategy_desc = {
	.name = "inr_switch",
	.strategy_init = inr_strategy_init,
	.strategy_release = inr_strategy_release,
	.strategy_alloc = inr_strategy_alloc,
	.strategy_get_data = inr_strategy_get_data,
};

int inr_strategy_register(void)
{
	return oplus_chg_strategy_register(&inr_strategy_desc);
}
