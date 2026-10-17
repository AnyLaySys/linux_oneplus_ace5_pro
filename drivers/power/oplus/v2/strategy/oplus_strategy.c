// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2021 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "[STRATEGY]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/device.h>
#include <oplus_chg.h>
#include <oplus_chg_module.h>
#include <oplus_strategy.h>

static DEFINE_MUTEX(list_lock);
static LIST_HEAD(strategy_list);

static struct oplus_chg_strategy_desc *strategy_desc_find_by_name(const char *name)
{
	struct oplus_chg_strategy_desc *desc;

	if (name == NULL) {
		return NULL;
	}

	mutex_lock(&list_lock);
	list_for_each_entry(desc, &strategy_list, list) {
		if (desc->name == NULL)
			continue;
		if (strcmp(desc->name, name) == 0) {
			mutex_unlock(&list_lock);
			return desc;
		}
	}
	mutex_unlock(&list_lock);

	return NULL;
}

struct oplus_chg_strategy *
oplus_chg_strategy_alloc(const char *name, unsigned char *buf, size_t size)
{
	struct oplus_chg_strategy *strategy;
	struct oplus_chg_strategy_desc *desc;

	if (name == NULL) {
		return NULL;
	}
	if (buf == NULL) {
		return NULL;
	}

	desc = strategy_desc_find_by_name(name);
	if (desc == NULL) {
		return NULL;
	}

	if (desc->strategy_alloc == NULL) {
		return NULL;
	}

	strategy = desc->strategy_alloc(buf, size);
	if (IS_ERR_OR_NULL(strategy)) {
		return NULL;
	}
	strategy->desc = desc;

	return strategy;
}

struct oplus_chg_strategy *
oplus_chg_strategy_alloc_by_node(const char *name, struct device_node *node)
{
	struct oplus_chg_strategy *strategy;
	struct oplus_chg_strategy_desc *desc;

	if (name == NULL) {
		return NULL;
	}
	if (node == NULL) {
		return NULL;
	}

	desc = strategy_desc_find_by_name(name);
	if (desc == NULL) {
		return NULL;
	}

	if (desc->strategy_alloc_by_node == NULL) {
		return NULL;
	}

	strategy = desc->strategy_alloc_by_node(node);
	if (IS_ERR_OR_NULL(strategy)) {
		return NULL;
	}
	strategy->desc = desc;

	return strategy;
}

#if IS_ENABLED(CONFIG_OPLUS_DYNAMIC_CONFIG_CHARGER)
struct oplus_chg_strategy *
oplus_chg_strategy_alloc_by_param_head(const char *name, const char *node_name, struct oplus_param_head *head)
{
	struct oplus_chg_strategy *strategy;
	struct oplus_chg_strategy_desc *desc;

	if (name == NULL) {
		return NULL;
	}
	if (node_name == NULL) {
		return NULL;
	}
	if (head == NULL) {
		return NULL;
	}

	desc = strategy_desc_find_by_name(name);
	if (desc == NULL) {
		return NULL;
	}

	if (desc->strategy_alloc_by_param_head == NULL) {
		return NULL;
	}

	strategy = desc->strategy_alloc_by_param_head(node_name, head);
	if (IS_ERR_OR_NULL(strategy)) {
		return NULL;
	}
	strategy->desc = desc;

	return strategy;
}
#endif

int oplus_chg_strategy_release(struct oplus_chg_strategy *strategy)
{
	if (strategy == NULL) {
		return -EINVAL;
	}

	strategy->desc->strategy_release(strategy);

	return 0;
}

int oplus_chg_strategy_init(struct oplus_chg_strategy *strategy)
{
	int rc;

	if (strategy == NULL) {
		return -EINVAL;
	}
	if (strategy->desc == NULL) {
		return -EINVAL;
	}

	rc = strategy->desc->strategy_init(strategy);
	if (rc < 0) {
		return rc;
	}
	strategy->initialized = true;

	return 0;
}

int oplus_chg_strategy_get_data(struct oplus_chg_strategy *strategy, void *ret)
{
	if (strategy == NULL) {
		return -EINVAL;
	}
	if (ret == NULL) {
		return -EINVAL;
	}

	return strategy->desc->strategy_get_data(strategy, ret);
}

int oplus_chg_strategy_set_process_data(struct oplus_chg_strategy *strategy, const char *type, unsigned long arg)
{
	if (strategy == NULL) {
		return -EINVAL;
	}
	if (type == NULL) {
		return -EINVAL;
	}
	if (!strategy->desc || !strategy->desc->strategy_set_process_data)
		return -ENOTSUPP;

	return strategy->desc->strategy_set_process_data(strategy, type, arg);
}

int oplus_chg_strategy_get_metadata(struct oplus_chg_strategy *strategy, void *ret)
{
	if (strategy == NULL) {
		return -EINVAL;
	}
	if (ret == NULL) {
		return -EINVAL;
	}

	if (!strategy->desc || !strategy->desc->strategy_get_metadata)
		return -ENOTSUPP;

	return strategy->desc->strategy_get_metadata(strategy, ret);
}

int oplus_chg_strategy_get_custom_data(struct oplus_chg_strategy *strategy, const char *type, void *ret)
{
	if (strategy == NULL) {
		return -EINVAL;
	}
	if (ret == NULL) {
		return -EINVAL;
	}

	if (!strategy->desc || !strategy->desc->strategy_get_custom_data)
		return -ENOTSUPP;

	return strategy->desc->strategy_get_custom_data(strategy, type, ret);
}

int oplus_chg_strategy_register(struct oplus_chg_strategy_desc *desc)
{
	struct oplus_chg_strategy_desc *desc_temp;

	if (desc == NULL) {
		return -EINVAL;
	}

	mutex_lock(&list_lock);
	list_for_each_entry(desc_temp, &strategy_list, list) {
		if (desc_temp->name == NULL)
			continue;
		if (strcmp(desc_temp->name, desc->name) == 0) {
			mutex_unlock(&list_lock);
			return -EINVAL;
		}
	}
	list_add(&desc->list, &strategy_list);
	mutex_unlock(&list_lock);

	return 0;
}

int oplus_chg_strategy_read_data(struct device *dev,
				 const char *prop_str, uint8_t **buf)
{
	struct device_node *node;
	int rc = 0, size;

	if (dev == NULL) {
		return -EINVAL;
	}
	if (buf == NULL) {
		return -EINVAL;
	}

	node = dev->of_node;
	rc = of_property_count_elems_of_size(node, prop_str, sizeof(u32));
	if (rc < 0) {
		return rc;
	}
	size = rc * sizeof(u32);
	if (size > PAGE_SIZE) {
		return -EINVAL;
	}

	*buf = devm_kzalloc(dev, size, GFP_KERNEL);
	if (*buf == NULL) {
		return -ENOMEM;
	}
	rc = of_property_read_u32_array(node, prop_str, (u32 *)*buf,
					size / sizeof(u32));
	if (rc) {
		return rc;
	}

	return size;
}

extern int cgcl_strategy_register(void);
extern int puc_strategy_register(void);
extern int lcf_strategy_register(void);
extern int puc2_strategy_register(void);
extern int ccd_strategy_register(void);
extern int inr_strategy_register(void);
extern int ddrc_strategy_register(void);
extern int ddrc_v2_strategy_register(void);
extern int bs_strategy_register(void);
extern int vfa_strategy_register(void);
extern int pcc_strategy_register(void);
extern int pcc_v2_strategy_register(void);

static __init int oplus_chg_strategy_module_init(void)
{
	cgcl_strategy_register();
	puc_strategy_register();
	lcf_strategy_register();
	puc2_strategy_register();
	inr_strategy_register();
	ddrc_strategy_register();
	ddrc_v2_strategy_register();
	bs_strategy_register();
	vfa_strategy_register();
	pcc_strategy_register();
	pcc_v2_strategy_register();
	ccd_strategy_register();

	return 0;
}

static __exit void oplus_chg_strategy_module_exit(void)
{
	struct oplus_chg_strategy_desc *desc, *tmp;

	mutex_lock(&list_lock);
	list_for_each_entry_safe(desc, tmp, &strategy_list, list) {
		list_del_init(&desc->list);
	}
	mutex_unlock(&list_lock);
}

oplus_chg_module_core_register(oplus_chg_strategy_module);
