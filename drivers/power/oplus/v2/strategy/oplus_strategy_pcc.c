// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2024 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "[STRATEGY_PCC]([%s][%d]): " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <oplus_chg.h>
#include <oplus_mms.h>
#include <oplus_mms_gauge.h>
#include <oplus_chg_comm.h>
#include <oplus_strategy.h>
#include "../voocphy/oplus_voocphy.h"

enum pcc_chg_protocol_type {
	PCC_UNKNOW_PROTOCOL,
	PCC_VOOC_PROTOCOL,
	PCC_INVALID_PROTOCOL,
};

struct pcc_strategy {
	struct oplus_chg_strategy strategy;
	enum pcc_chg_protocol_type chg_protocol;

	int ibus_target;
	int ibus_req;

	unsigned long curr_down_moment;
	struct mutex lock;
};

static struct oplus_chg_strategy *
pcc_strategy_alloc(unsigned char *buf, size_t size)
{
	return ERR_PTR(-ENOTSUPP);
}

static struct oplus_chg_strategy *
pcc_strategy_alloc_by_node(struct device_node *node)
{
	int rc;
	u32 data;
	struct pcc_strategy *pcc;

	if (node == NULL) {
		return ERR_PTR(-EINVAL);
	}

	pcc = kzalloc(sizeof(struct pcc_strategy), GFP_KERNEL);
	if (pcc == NULL) {
		return ERR_PTR(-ENOMEM);
	}

	rc = of_property_read_u32(node, "oplus,chg_protocol", &data);
	if (rc < 0) {
		kfree(pcc);
		return ERR_PTR(-EINVAL);
	} else {
		if (data >= PCC_INVALID_PROTOCOL || data <= PCC_UNKNOW_PROTOCOL) {
			kfree(pcc);
			return ERR_PTR(-EINVAL);
		}
		pcc->chg_protocol = data;
	}

	mutex_init(&pcc->lock);

	return (struct oplus_chg_strategy *)pcc;
}

static int pcc_strategy_release(struct oplus_chg_strategy *strategy)
{
	struct pcc_strategy *pcc;

	if (strategy == NULL) {
		return -EINVAL;
	}
	pcc = (struct pcc_strategy *)strategy;

	kfree(pcc);

	return 0;
}

static int pcc_strategy_init(struct oplus_chg_strategy *strategy)
{
	struct pcc_strategy *pcc;

	if (strategy == NULL) {
		return -EINVAL;
	}
	pcc = (struct pcc_strategy *)strategy;

	pcc->ibus_req = 10; /*dafault 1500ma*/
	pcc->ibus_target = 0;
	pcc->curr_down_moment = 0;


	return 0;
}

static int pcc_strategy_update_ibus_target(struct pcc_strategy *pcc, int ibus_target)
{
	mutex_lock(&pcc->lock);
	pcc->ibus_target = ibus_target;
	if (pcc->ibus_req > ibus_target) {
		pcc->ibus_req = ibus_target;
		pcc->curr_down_moment = (jiffies * 1000) / HZ;
	}
	mutex_unlock(&pcc->lock);

	return 0;
}

static int pcc_strategy_cal_step_curr(struct pcc_strategy *pcc)
{
	if (pcc->ibus_req >= pcc->ibus_target) {
		pcc->ibus_req = pcc->ibus_target;
	} else {
		pcc->ibus_req += 1; /* add 200ma one time */
		if (pcc->ibus_req > pcc->ibus_target)
			pcc->ibus_req = pcc->ibus_target;
	}

	return 0;
}

static int pcc_strategy_update_ibus_req(struct pcc_strategy *pcc)
{
	unsigned long curr_moment = (jiffies * 1000) / HZ;

	mutex_lock(&pcc->lock);
	if (pcc->ibus_target == pcc->ibus_req) {
		mutex_unlock(&pcc->lock);
		return 0;
	}

	if (pcc->curr_down_moment && curr_moment < (pcc->curr_down_moment + 100)) {
		mutex_unlock(&pcc->lock);
		return 0;
	}

	pcc_strategy_cal_step_curr(pcc);

	mutex_unlock(&pcc->lock);

	return 0;
}

static int pcc_strategy_set_process_data(struct oplus_chg_strategy *strategy, const char *type, unsigned long arg)
{
	struct pcc_strategy *pcc;

	if (strategy == NULL) {
		return -EINVAL;
	}

	if (type == NULL) {
		return -EINVAL;
	}

	pcc = (struct pcc_strategy *)strategy;

	if (strncmp(type, "curr_target", sizeof("curr_target")) == 0)
		pcc_strategy_update_ibus_target(pcc, arg);
	else if (strncmp(type, "curr_pcc_cycle_t", sizeof("curr_pcc_cycle_t")) == 0)
		pcc_strategy_update_ibus_req(pcc);
	else
		return -ENOTSUPP;

	return 0;
}

static int pcc_strategy_get_data(struct oplus_chg_strategy *strategy, void *ret)
{
	struct pcc_strategy *pcc;

	if (strategy == NULL) {
		return -EINVAL;
	}

	if (ret == NULL) {
		return -EINVAL;
	}

	pcc = (struct pcc_strategy *)strategy;

	mutex_lock(&pcc->lock);
	*((char *)ret) = pcc->ibus_req;
	mutex_unlock(&pcc->lock);

	return 0;
}

static struct oplus_chg_strategy_desc pcc_strategy_desc = {
	.name = "pcc_strategy",
	.strategy_init = pcc_strategy_init,
	.strategy_release = pcc_strategy_release,
	.strategy_alloc = pcc_strategy_alloc,
	.strategy_alloc_by_node = pcc_strategy_alloc_by_node,
	.strategy_get_data = pcc_strategy_get_data,
	.strategy_set_process_data = pcc_strategy_set_process_data,
	.strategy_get_metadata = NULL,
};

int pcc_strategy_register(void)
{
	return oplus_chg_strategy_register(&pcc_strategy_desc);
}

