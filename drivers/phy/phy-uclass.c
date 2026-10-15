// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 Texas Instruments Incorporated - https://www.ti.com/
 * Written by Jean-Jacques Hiblot  <jjhiblot@ti.com>
 */
// #define DEBUG

#define LOG_CATEGORY UCLASS_PHY
#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device.h> // devres.h
#include <generic-phy.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <power/regulator.h>

/**
 * struct phy_counts - Init and power-on counts of a single PHY port
 *
 * This structure is used to keep track of PHY initialization and power
 * state change requests, so that we don't power off and deinitialize a
 * PHY instance until all of its users want it done. Otherwise, multiple
 * consumers using the same PHY port can cause problems (e.g. one might
 * call power_off() after another's exit() and hang indefinitely).
 *
 * @id: The PHY ID within a PHY provider
 * @power_on_count: Times generic_phy_power_on() was called for this ID
 *                  without a matching generic_phy_power_off() afterwards
 * @init_count: Times generic_phy_init() was called for this ID
 *              without a matching generic_phy_exit() afterwards
 * @list: Handle for a linked list of these structures corresponding to
 *        ports of the same PHY provider
 * @supply: Handle to a phy-supply device
 */
struct phy_counts {
	unsigned long id;
	int power_on_count;
	int init_count;
	struct list_head list;
	struct udevice *supply;
};

static inline struct phy_ops *phy_dev_ops(struct udevice *dev)
{
	return (struct phy_ops *)dev->driver->ops;
}

static struct phy_counts *phy_get_counts(struct phy *phy)
{
	struct list_head *uc_priv;
	struct phy_counts *counts;

	if (!generic_phy_valid(phy))
		return NULL;

	uc_priv = dev_get_uclass_priv(phy->dev);
	list_for_each_entry(counts, uc_priv, list)
		if (counts->id == phy->id)
			return counts;

	return NULL;
}

static int phy_alloc_counts(struct phy *phy, struct udevice *supply)
{
	struct list_head *uc_priv;
	struct phy_counts *counts;

	if (!generic_phy_valid(phy))
		return 0;
	if (phy_get_counts(phy))
		return 0;

	uc_priv = dev_get_uclass_priv(phy->dev);
	counts = kzalloc(sizeof(*counts), GFP_KERNEL);
	if (!counts)
		return -ENOMEM;

	counts->id = phy->id;
	counts->power_on_count = 0;
	counts->init_count = 0;
	counts->supply = supply;
	list_add(&counts->list, uc_priv);

	return 0;
}

static int phy_uclass_pre_probe(struct udevice *dev)
{
	struct list_head *uc_priv = dev_get_uclass_priv(dev);

	INIT_LIST_HEAD(uc_priv);

	return 0;
}

static int phy_uclass_pre_remove(struct udevice *dev)
{
	struct list_head *uc_priv = dev_get_uclass_priv(dev);
	struct phy_counts *counts, *next;

	list_for_each_entry_safe(counts, next, uc_priv, list)
		kfree(counts);

	return 0;
}

static int generic_phy_xlate_offs_flags(struct phy *phy,
					struct ofnode_phandle_args *args)
{
	debug("%s(phy=%p)\n", __func__, phy);

	if (args->args_count > 1) {
		debug("Invalid args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	if (args->args_count)
		phy->id = args->args[0];
	else
		phy->id = 0;

	return 0;
}

int generic_phy_get_by_index_nodev(ofnode node, int index, struct phy *phy)
{
	struct ofnode_phandle_args args;
	struct phy_ops *ops;
	struct udevice *phydev, *supply = NULL;
	int i, ret;
	
	printf("%s\n", __func__);
	
	if(!ofnode_valid(node)) {
		log_err("bad node for phy %s\n", phy->dev->name);
		return -EINVAL;
	}

	debug("node=%s, index=%d, phy=%p\n", node.np->full_name, index, phy);

	assert(phy);
	phy->dev = NULL;
	ret = ofnode_parse_phandle_with_args(node, "phys", "#phy-cells", 0,
					     index, &args);
	if (ret) {
		printf("dev_read_phandle_with_args failed: err=%d\n", ret);
		return ret;
	}

	ret = uclass_get_device_by_ofnode(UCLASS_PHY, args.node, &phydev);
	if (ret) {
		printf("uclass_get_device_by_ofnode (%s) failed: err=%d\n", args.node.np->full_name, ret);

		/* Check if args.node's parent is a PHY provider */
		ret = uclass_get_device_by_ofnode(UCLASS_PHY, ofnode_get_parent(args.node),
						  &phydev);
		if (ret) {
			printf("get phy device for %s failed %d\n", args.node.np->full_name, ret);
			return ret;
		}	
		/* insert phy idx at first position into args array */
		for (i = args.args_count; i >= 1 ; i--)
			args.args[i] = args.args[i - 1];

		args.args_count++;
		args.args[0] = ofnode_read_u32_default(args.node, "reg", -1);
	}

	phy->dev = phydev;

	ops = phy_dev_ops(phydev);
	if(ops == NULL) {
		printf("null ops of phy %s node %s\n", phydev->name, phydev->node.np->full_name);
	}
	if (ops != NULL && ops->of_xlate)
		ret = ops->of_xlate(phy, &args);
	else
		ret = generic_phy_xlate_offs_flags(phy, &args);
	if (ret) {
		printf("of_xlate() failed: %d\n", ret);
		goto err;
	}

	if (CONFIG_IS_ENABLED(DM_REGULATOR)) {
		ret = device_get_supply_regulator(phydev, "phy-supply",
						  &supply);
		if (ret && ret != -ENOENT) {
			printf("device_get_supply_regulator failed: %d\n", ret);
			goto err;
		}
	}

	ret = phy_alloc_counts(phy, supply);
	if (ret) {
		printf("phy_alloc_counts() failed: %d\n", ret);
		goto err;
	}

	return 0;

err:
	phy->dev = NULL;
	return ret;
}

int generic_phy_get_by_index(struct udevice *dev, int index, struct phy *phy)
{
	debug("dev %s %d\n", dev->name, index);
	return generic_phy_get_by_index_nodev(dev_ofnode(dev), index, phy);
}

int generic_phy_get_by_name(struct udevice *dev, const char *phy_name,
			    struct phy *phy)
{
	int index;

	debug("%s(dev=%p, name=%s, phy=%p)\n", __func__, dev, phy_name, phy);

	assert(phy);
	phy->dev = NULL;

	index = dev_read_stringlist_search(dev, "phy-names", phy_name);
	if (index < 0) {
		debug("dev_read_stringlist_search() failed: %d\n", index);
		return index;
	}

	return generic_phy_get_by_index(dev, index, phy);
}

int generic_phy_init(struct phy *phy)
{
	struct phy_counts *counts;
	struct phy_ops const *ops;
	int ret;

	if (!generic_phy_valid(phy))
		return 0;
	counts = phy_get_counts(phy);
	if (counts->init_count > 0) {
		counts->init_count++;
		return 0;
	}

	ops = phy_dev_ops(phy->dev);
	if (ops->init) {
		ret = ops->init(phy);
		if (ret) {
			dev_err(phy->dev, "PHY: Failed to init %s: %d.\n",
				phy->dev->name, ret);
			return ret;
		}
	}
	counts->init_count = 1;

	return 0;
}

int generic_phy_reset(struct phy *phy)
{
	struct phy_ops const *ops;
	int ret;

	if (!generic_phy_valid(phy))
		return 0;
	ops = phy_dev_ops(phy->dev);
	if (!ops->reset)
		return 0;
	ret = ops->reset(phy);
	if (ret)
		dev_err(phy->dev, "PHY: Failed to reset %s: %d.\n",
			phy->dev->name, ret);

	return ret;
}

int generic_phy_exit(struct phy *phy)
{
	struct phy_counts *counts;
	struct phy_ops const *ops;
	int ret;

	if (!generic_phy_valid(phy))
		return 0;
	counts = phy_get_counts(phy);
	if (counts->init_count == 0)
		return 0;
	if (counts->init_count > 1) {
		counts->init_count--;
		return 0;
	}

	ops = phy_dev_ops(phy->dev);
	if (ops->exit) {
		ret = ops->exit(phy);
		if (ret) {
			dev_err(phy->dev, "PHY: Failed to exit %s: %d.\n",
				phy->dev->name, ret);
			return ret;
		}
	}
	counts->init_count = 0;

	return 0;
}

int generic_phy_power_on(struct phy *phy)
{
	struct phy_counts *counts;
	struct phy_ops const *ops;
	int ret;

	if (!generic_phy_valid(phy))
		return 0;
	counts = phy_get_counts(phy);
	if (counts->power_on_count > 0) {
		counts->power_on_count++;
		return 0;
	}

	ret = regulator_set_enable_if_allowed(counts->supply, true);
	if (ret && ret != -ENOSYS) {
		dev_err(phy->dev, "PHY: Failed to enable regulator %s: %d.\n",
			counts->supply->name, ret);
		return ret;
	}

	ops = phy_dev_ops(phy->dev);
	if (ops->power_on) {
		ret = ops->power_on(phy);
		if (ret) {
			dev_err(phy->dev, "PHY: Failed to power on %s: %d.\n",
				phy->dev->name, ret);
			regulator_set_enable_if_allowed(counts->supply, false);
			return ret;
		}
	}
	counts->power_on_count = 1;

	return 0;
}

int generic_phy_power_off(struct phy *phy)
{
	struct phy_counts *counts;
	struct phy_ops const *ops;
	int ret;

	if (!generic_phy_valid(phy))
		return 0;
	counts = phy_get_counts(phy);
	if (counts->power_on_count == 0)
		return 0;
	if (counts->power_on_count > 1) {
		counts->power_on_count--;
		return 0;
	}

	ops = phy_dev_ops(phy->dev);
	if (ops->power_off) {
		ret = ops->power_off(phy);
		if (ret) {
			dev_err(phy->dev, "PHY: Failed to power off %s: %d.\n",
				phy->dev->name, ret);
			return ret;
		}
	}
	counts->power_on_count = 0;

	ret = regulator_set_enable_if_allowed(counts->supply, false);
	if (ret && ret != -ENOSYS)
		dev_err(phy->dev, "PHY: Failed to disable regulator %s: %d.\n",
			counts->supply->name, ret);

	return 0;
}

int generic_phy_configure(struct phy *phy, void *params)
{
	struct phy_ops const *ops;

	if (!generic_phy_valid(phy))
		return 0;
	ops = phy_dev_ops(phy->dev);

	return ops->configure ? ops->configure(phy, params) : 0;
}

int generic_phy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct phy_ops const *ops;

	if (!generic_phy_valid(phy))
		return 0;
	ops = phy_dev_ops(phy->dev);
	
	// make old happy backport2025
	phy->attrs.mode = mode;

	return ops->set_mode ? ops->set_mode(phy, mode, submode) : 0;
}

int generic_phy_set_speed(struct phy *phy, int speed)
{
	struct phy_ops const *ops;

	if (!generic_phy_valid(phy))
		return 0;
	ops = phy_dev_ops(phy->dev);

	return ops->set_speed ? ops->set_speed(phy, speed) : 0;
}

int generic_phy_get_bulk(struct udevice *dev, struct phy_bulk *bulk)
{
	int i, ret, count;
	struct udevice *phydev = dev;

	bulk->count = 0;

	/* Return if no phy declared */
	if (!dev_read_prop(dev, "phys", NULL)) {
		phydev = dev->parent;
		if (!dev_read_prop(phydev, "phys", NULL)) {
			dev_err(dev, "%s : no phys property\n", __func__);
			return 0;
		}
	}

	count = dev_count_phandle_with_args(phydev, "phys", "#phy-cells", 0);
	if (count < 1) {
		dev_err(dev, "%s : no phys found %d\n", __func__, count);
		return count;
	}

	bulk->phys = devm_kcalloc(phydev, count, sizeof(struct phy), GFP_KERNEL);
	if (!bulk->phys)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		ret = generic_phy_get_by_index(phydev, i, &bulk->phys[i]);
		if (ret) {
			dev_err(dev, "Failed to get phy-%d of total %d for %s%d %s\n", 
				i, count, dev->name, dev->seq, dev->node.np->full_name);
			return ret;
		}
		bulk->count++;
	}

	return 0;
}

int generic_phy_init_bulk(struct phy_bulk *bulk)
{
	struct phy *phys = bulk->phys;
	int i, ret;

	for (i = 0; i < bulk->count; i++) {
		ret = generic_phy_init(&phys[i]);
		if (ret) {
			pr_err("Can't init PHY%d\n", i);
			goto phys_init_err;
		}
	}

	return 0;

phys_init_err:
	for (; i > 0; i--)
		generic_phy_exit(&phys[i - 1]);

	return ret;
}

int generic_phy_exit_bulk(struct phy_bulk *bulk)
{
	struct phy *phys = bulk->phys;
	int i, ret = 0;

	for (i = 0; i < bulk->count; i++)
		ret |= generic_phy_exit(&phys[i]);

	return ret;
}

int generic_phy_power_on_bulk(struct phy_bulk *bulk)
{
	struct phy *phys = bulk->phys;
	int i, ret;

	for (i = 0; i < bulk->count; i++) {
		ret = generic_phy_power_on(&phys[i]);
		if (ret) {
			pr_err("Can't power on PHY%d\n", i);
			goto phys_poweron_err;
		}
	}

	return 0;

phys_poweron_err:
	for (; i > 0; i--)
		generic_phy_power_off(&phys[i - 1]);

	return ret;
}

int generic_phy_power_off_bulk(struct phy_bulk *bulk)
{
	struct phy *phys = bulk->phys;
	int i, ret = 0;

	for (i = 0; i < bulk->count; i++)
		ret |= generic_phy_power_off(&phys[i]);

	return ret;
}

int generic_setup_phy(struct udevice *dev, struct phy *phy, int index,
		      enum phy_mode mode, int submode)
{
	int ret;

	ret = generic_phy_get_by_index(dev, index, phy);
	if (ret)
		return ret == -ENOENT ? 0 : ret;

	ret = generic_phy_init(phy);
	if (ret)
		return ret;

	ret = generic_phy_set_mode(phy, mode, submode);
	if (ret)
		goto phys_mode_err;

	ret = generic_phy_power_on(phy);
	if (ret)
		goto phys_mode_err;

	return 0;

phys_mode_err:
	generic_phy_exit(phy);
	return ret;
}

int generic_shutdown_phy(struct phy *phy)
{
	int ret;

	if (!generic_phy_valid(phy))
		return 0;

	ret = generic_phy_power_off(phy);
	if (ret)
		return ret;

	return generic_phy_exit(phy);
}
// per_device_auto

UCLASS_DRIVER(phy) = {
	.id		= UCLASS_PHY,
	.name		= "phy",
	.pre_probe	= phy_uclass_pre_probe,
	.pre_remove	= phy_uclass_pre_remove,
	.per_device_auto_alloc_size = sizeof(struct list_head),
};
