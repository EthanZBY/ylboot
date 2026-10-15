// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip PIPE USB3.0 PCIE SATA Combo Phy driver
 *
 * Copyright (C) 2021 Rockchip Electronics Co., Ltd.
 * Port to U-Boot by Raymond on 2025-8-12 at GSL.
 */
#define DEBUG

#include <common.h>
#include <dt-bindings/phy/phy.h>
#include <dm.h>
#include <dm/lists.h>
#include <generic-phy.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <reset-uclass.h>
#include <power/regulator.h>
#include <linux/units.h>
#include <linux/iopoll.h>
#include <clk.h>

#ifdef LINUX
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#else 
	#define reset_control_assert reset_assert
	#define reset_control_deassert reset_deassert
#endif

#define BIT_WRITEABLE_SHIFT		16
#define REF_CLOCK_24MHz			(24 * HZ_PER_MHZ)
#define REF_CLOCK_25MHz			(25 * HZ_PER_MHZ)
#define REF_CLOCK_100MHz		(100 * HZ_PER_MHZ)

/* COMBO PHY REG */
#define PHYREG6				0x14
#define PHYREG6_PLL_DIV_MASK		GENMASK(7, 6)
#define PHYREG6_PLL_DIV_SHIFT		6
#define PHYREG6_PLL_DIV_2		1

#define PHYREG7				0x18
#define PHYREG7_TX_RTERM_MASK		GENMASK(7, 4)
#define PHYREG7_TX_RTERM_SHIFT		4
#define PHYREG7_TX_RTERM_50OHM		8
#define PHYREG7_RX_RTERM_MASK		GENMASK(3, 0)
#define PHYREG7_RX_RTERM_SHIFT		0
#define PHYREG7_RX_RTERM_44OHM		15

#define PHYREG8				0x1C
#define PHYREG8_SSC_EN			BIT(4)

#define PHYREG11			0x28
#define PHYREG11_SU_TRIM_0_7		0xF0

#define PHYREG12			0x2C
#define PHYREG12_PLL_LPF_ADJ_VALUE	4

#define PHYREG13			0x30
#define PHYREG13_RESISTER_MASK		GENMASK(5, 4)
#define PHYREG13_RESISTER_SHIFT		0x4
#define PHYREG13_RESISTER_HIGH_Z	3
#define PHYREG13_CKRCV_AMP0		BIT(7)

#define PHYREG14			0x34
#define PHYREG14_CKRCV_AMP1		BIT(0)

#define PHYREG15			0x38
#define PHYREG15_CTLE_EN		BIT(0)
#define PHYREG15_SSC_CNT_MASK		GENMASK(7, 6)
#define PHYREG15_SSC_CNT_SHIFT		6
#define PHYREG15_SSC_CNT_VALUE		1

#define PHYREG16			0x3C
#define PHYREG16_SSC_CNT_VALUE		0x5f

#define PHYREG18			0x44
#define PHYREG18_PLL_LOOP		0x32

#define PHYREG27			0x6C
#define PHYREG27_RX_TRIM_RK3588		0x4C

#define PHYREG32			0x7C
#define PHYREG32_SSC_MASK		GENMASK(7, 4)
#define PHYREG32_SSC_DIR_SHIFT		4
#define PHYREG32_SSC_UPWARD		0
#define PHYREG32_SSC_DOWNWARD		1
#define PHYREG32_SSC_OFFSET_SHIFT	6
#define PHYREG32_SSC_OFFSET_500PPM	1

#define PHYREG33			0x80
#define PHYREG33_PLL_KVCO_MASK		GENMASK(4, 2)
#define PHYREG33_PLL_KVCO_SHIFT		2
#define PHYREG33_PLL_KVCO_VALUE		2

struct rockchip_combphy_priv;

struct combphy_reg {
	u16 offset;
	u16 bitend;
	u16 bitstart;
	u16 disable;
	u16 enable;
};

struct rockchip_combphy_grfcfg {
	struct combphy_reg pcie_mode_set;
	struct combphy_reg usb_mode_set;
	struct combphy_reg sgmii_mode_set;
	struct combphy_reg qsgmii_mode_set;
	struct combphy_reg pipe_rxterm_set;
	struct combphy_reg pipe_txelec_set;
	struct combphy_reg pipe_txcomp_set;
	struct combphy_reg pipe_clk_25m;
	struct combphy_reg pipe_clk_100m;
	struct combphy_reg pipe_phymode_sel;
	struct combphy_reg pipe_rate_sel;
	struct combphy_reg pipe_rxterm_sel;
	struct combphy_reg pipe_txelec_sel;
	struct combphy_reg pipe_txcomp_sel;
	struct combphy_reg pipe_clk_ext;
	struct combphy_reg pipe_sel_usb;
	struct combphy_reg pipe_sel_qsgmii;
	struct combphy_reg pipe_phy_status;
	struct combphy_reg con0_for_pcie;
	struct combphy_reg con1_for_pcie;
	struct combphy_reg con2_for_pcie;
	struct combphy_reg con3_for_pcie;
	struct combphy_reg con0_for_sata;
	struct combphy_reg con1_for_sata;
	struct combphy_reg con2_for_sata;
	struct combphy_reg con3_for_sata;
	struct combphy_reg pipe_con0_for_sata;
	struct combphy_reg pipe_con1_for_sata;
	struct combphy_reg pipe_xpcs_phy_ready;
	struct combphy_reg pipe_pcie1l0_sel;
	struct combphy_reg pipe_pcie1l1_sel;
};

struct rockchip_combphy_cfg {
	unsigned int num_phys;
	unsigned int phy_ids[3];
	const struct rockchip_combphy_grfcfg *grfcfg;
	int (*combphy_cfg)(struct rockchip_combphy_priv *priv);
};

struct rockchip_combphy_priv {
	u8 type;
	int id;
	void __iomem *mmio;
	int num_clks;
	struct clk_bulk *clks;
	struct udevice *dev;
	struct regmap *pipe_grf;
	struct regmap *phy_grf;
	struct phy *phy;
	struct reset_ctl phy_rst; // changed from reset_control* by Raymond
	const struct rockchip_combphy_cfg *cfg;
	bool enable_ssc;
	bool ext_refclk;
	struct clk *refclk;
};

static void rockchip_combphy_updatel(struct rockchip_combphy_priv *priv,
				     int mask, int val, int reg)
{
	unsigned int temp;

	temp = readl(priv->mmio + reg);
	temp = (temp & ~(mask)) | val;
	writel(temp, priv->mmio + reg);
}

static int rockchip_combphy_param_write(struct regmap *base,
					const struct combphy_reg *reg, bool en)
{
	u32 val, mask, tmp;

	tmp = en ? reg->enable : reg->disable;
	mask = GENMASK(reg->bitend, reg->bitstart);
	val = (tmp << reg->bitstart) | (mask << BIT_WRITEABLE_SHIFT);

	return regmap_write(base, reg->offset, val);
}

static u32 rockchip_combphy_is_ready(struct rockchip_combphy_priv *priv)
{
	const struct rockchip_combphy_grfcfg *cfg = priv->cfg->grfcfg;
	u32 mask, val;

	mask = GENMASK(cfg->pipe_phy_status.bitend,
		       cfg->pipe_phy_status.bitstart);

	regmap_read(priv->phy_grf, cfg->pipe_phy_status.offset, &val);
	val = (val & mask) >> cfg->pipe_phy_status.bitstart;

	return val;
}

static int rockchip_combphy_init(struct phy *phy)
{
	struct udevice *parent = phy->dev->parent;
	struct rockchip_combphy_priv *priv = dev_get_priv(parent);
	const struct rockchip_combphy_grfcfg *cfg = priv->cfg->grfcfg;
	u32 val;
	int ret;

	ret = clk_enable_bulk(priv->clks); //clk_bulk_prepare_enable(priv->num_clks, priv->clks);
	if (ret) {
		dev_err(priv->dev, "failed to enable clks\n");
		return ret;
	}

	switch (priv->type) {
	case PHY_TYPE_PCIE:
	case PHY_TYPE_USB3:
	case PHY_TYPE_SATA:
	case PHY_TYPE_SGMII:
	case PHY_TYPE_QSGMII:
		if (priv->cfg->combphy_cfg)
			ret = priv->cfg->combphy_cfg(priv);
		break;
	default:
		dev_err(priv->dev, "incompatible PHY type\n");
		ret = -EINVAL;
		break;
	}

	if (ret) {
		dev_err(priv->dev, "failed to init phy for phy type %x\n", priv->type);
		goto err_clk;
	}

	ret = reset_control_deassert(&priv->phy_rst);
	if (ret)
		goto err_clk;

	if (priv->type == PHY_TYPE_USB3) {
		// readx_poll_timeout_atomic(op, addr, val, cond, delay_us, timeout_us) 
		ret = readx_poll_timeout/*_atomic*/(rockchip_combphy_is_ready,
						priv, val,
						val == cfg->pipe_phy_status.enable,
						/*10, */1000);
		if (ret)
			dev_warn(priv->dev, "wait phy status ready timeout\n");
	}

	return 0;

err_clk:
	clk_disable_bulk(priv->clks); // clk_bulk_disable_unprepare

	return ret;
}

static int rockchip_combphy_exit(struct phy *phy)
{
	struct rockchip_combphy_priv *priv = dev_get_priv(phy->dev->parent); // phy_get_drvdata(phy);

	clk_disable_bulk(priv->clks);
	reset_control_assert(&priv->phy_rst);

	return 0;
}

static const struct phy_ops rockchip_combphy_ops = {
	.init = rockchip_combphy_init,
	.exit = rockchip_combphy_exit,
};

static struct phy *rockchip_combphy_xlate(struct udevice *dev, const struct of_phandle_args *args)
{
	struct rockchip_combphy_priv *priv = dev_get_priv(dev);//dev_get_drvdata(dev);

	if (args->args_count != 1) {
		dev_err(dev, "invalid number of arguments\n");
		return ERR_PTR(-EINVAL);
	}

	if (priv->type != PHY_NONE && priv->type != args->args[0])
		dev_warn(dev, "phy type select %d overwriting type %d\n",
			 args->args[0], priv->type);

	priv->type = args->args[0];

	return priv->phy;
}

static int rockchip_combphy_parse_dt(struct udevice *dev, struct rockchip_combphy_priv *priv)
{
	int i, ret;

	ret = clk_get_bulk(dev, &priv->clks);// priv->num_clks = devm_clk_bulk_get_all(dev, &priv->clks);
	if (ret != 0)
		return -EINVAL;

	priv->num_clks = priv->clks->count;
	i = ofnode_stringlist_search(dev_ofnode(dev), "clock-names", "refclk");
	if(i >= 0 && i < priv->clks->count) {
		priv->refclk = &priv->clks[i];
	}
	else {
		dev_err(dev, "no refclk found %d\n", i);
		return -EINVAL;
	}

	priv->pipe_grf = syscon_regmap_lookup_by_phandle(dev, "rockchip,pipe-grf");
	if (IS_ERR(priv->pipe_grf)) {
		dev_err(dev, "failed to find peri_ctrl pipe-grf regmap\n");
		return PTR_ERR(priv->pipe_grf);
	}

	priv->phy_grf = syscon_regmap_lookup_by_phandle(dev, "rockchip,pipe-phy-grf");
	if (IS_ERR(priv->phy_grf)) {
		dev_err(dev, "failed to find peri_ctrl pipe-phy-grf regmap\n");
		return PTR_ERR(priv->phy_grf);
	}

	priv->enable_ssc = /*device_property_present*/ofnode_read_bool(dev_ofnode(dev), "rockchip,enable-ssc");

	priv->ext_refclk = /*device_property_present*/ofnode_read_bool(dev_ofnode(dev), "rockchip,ext-refclk");

	ret = reset_get_by_index(dev, 0, &priv->phy_rst);//devm_reset_control_array_get_exclusive(dev);
	if (ret != 0)
		dev_err(dev, "failed to get phy reset %d\n", ret);

	return ret;
}
static int rockchip_combphy_bind(struct udevice *dev)
{
	struct udevice *child;
	ofnode subnode;
	const char *node_name;
	int ret;
	
	debug("binding %s\n", dev->name);

	dev_for_each_subnode(subnode, dev) {
		if (!ofnode_valid(subnode)) {
			debug("%s: %s subnode not found", __func__, dev->name);
			return -ENXIO;
		}

		node_name = ofnode_get_name(subnode);
		debug("%s: subnode %s\n", __func__, node_name);

		ret = device_bind_driver_to_node(dev, "naneng_combphy_port",
						 node_name, subnode, &child);
		if (ret) {
			pr_err("%s: '%s' cannot bind 'naneng_combphy_port'\n",
			       __func__, node_name);
			return ret;
		}
	}

	return 0;
}

static int rockchip_combphy_probe(struct udevice *dev)
{
	struct phy_provider *phy_provider;
	struct rockchip_combphy_priv *priv = dev_get_priv(dev);
	const struct rockchip_combphy_cfg *phy_cfg;
	int ret, id;

	debug("%s %s\n",dev->name, dev->node.np->full_name);
	
	phy_cfg = (const struct rockchip_combphy_cfg *)dev_get_driver_data(dev);
	if (!phy_cfg) {
		dev_err(dev, "no OF match data provided\n");
		return -EINVAL;
	}

#ifdef LINUX
	/* find the phy-id from the io address */
	priv->id = -ENODEV;
	for (id = 0; id < phy_cfg->num_phys; id++) {
		if (res->start == phy_cfg->phy_ids[id]) {
			priv->id = id;
			break;
		}
	}
#endif
	priv->dev = dev;
	priv->type = PHY_NONE;
	priv->cfg = phy_cfg;

	ret = rockchip_combphy_parse_dt(dev, priv);
	if (ret)
		return ret;

	ret = reset_control_assert(&priv->phy_rst);
	if (ret) {
		dev_err(dev, "failed to reset phy\n");
		return ret;
	}

	// dev_set_drvdata(dev, priv);
	// phy_set_drvdata(priv->phy, priv);

	// phy_provider = devm_of_phy_provider_register(dev, rockchip_combphy_xlate);

	return 0; // PTR_ERR_OR_ZERO(phy_provider);
}

static int rk3568_combphy_cfg(struct rockchip_combphy_priv *priv)
{
	const struct rockchip_combphy_grfcfg *cfg = priv->cfg->grfcfg;
	unsigned long rate;
	u32 val;

	switch (priv->type) {
	case PHY_TYPE_PCIE:
		/* Set SSC downward spread spectrum. */
		rockchip_combphy_updatel(priv, PHYREG32_SSC_MASK,
					 PHYREG32_SSC_DOWNWARD << PHYREG32_SSC_DIR_SHIFT,
					 PHYREG32);

		rockchip_combphy_param_write(priv->phy_grf, &cfg->con0_for_pcie, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con1_for_pcie, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con2_for_pcie, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con3_for_pcie, true);
		break;

	case PHY_TYPE_USB3:
		/* Set SSC downward spread spectrum. */
		rockchip_combphy_updatel(priv, PHYREG32_SSC_MASK,
					 PHYREG32_SSC_DOWNWARD << PHYREG32_SSC_DIR_SHIFT,
					 PHYREG32);

		/* Enable adaptive CTLE for USB3.0 Rx. */
		val = readl(priv->mmio + PHYREG15);
		val |= PHYREG15_CTLE_EN;
		writel(val, priv->mmio + PHYREG15);

		/* Set PLL KVCO fine tuning signals. */
		rockchip_combphy_updatel(priv, PHYREG33_PLL_KVCO_MASK,
					 PHYREG33_PLL_KVCO_VALUE << PHYREG33_PLL_KVCO_SHIFT,
					 PHYREG33);

		/* Enable controlling random jitter. */
		writel(PHYREG12_PLL_LPF_ADJ_VALUE, priv->mmio + PHYREG12);

		/* Set PLL input clock divider 1/2. */
		rockchip_combphy_updatel(priv, PHYREG6_PLL_DIV_MASK,
					 PHYREG6_PLL_DIV_2 << PHYREG6_PLL_DIV_SHIFT,
					 PHYREG6);

		writel(PHYREG18_PLL_LOOP, priv->mmio + PHYREG18);
		writel(PHYREG11_SU_TRIM_0_7, priv->mmio + PHYREG11);

		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_sel_usb, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_txcomp_sel, false);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_txelec_sel, false);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->usb_mode_set, true);
		break;

	case PHY_TYPE_SATA:
		/* Enable adaptive CTLE for SATA Rx. */
		val = readl(priv->mmio + PHYREG15);
		val |= PHYREG15_CTLE_EN;
		writel(val, priv->mmio + PHYREG15);
		/*
		 * Set tx_rterm=50ohm and rx_rterm=44ohm for SATA.
		 * 0: 60ohm, 8: 50ohm 15: 44ohm (by step abort 1ohm)
		 */
		val = PHYREG7_TX_RTERM_50OHM << PHYREG7_TX_RTERM_SHIFT;
		val |= PHYREG7_RX_RTERM_44OHM << PHYREG7_RX_RTERM_SHIFT;
		writel(val, priv->mmio + PHYREG7);

		rockchip_combphy_param_write(priv->phy_grf, &cfg->con0_for_sata, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con1_for_sata, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con2_for_sata, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con3_for_sata, true);
		rockchip_combphy_param_write(priv->pipe_grf, &cfg->pipe_con0_for_sata, true);
		break;

	case PHY_TYPE_SGMII:
		rockchip_combphy_param_write(priv->pipe_grf, &cfg->pipe_xpcs_phy_ready, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_phymode_sel, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_sel_qsgmii, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->sgmii_mode_set, true);
		break;

	case PHY_TYPE_QSGMII:
		rockchip_combphy_param_write(priv->pipe_grf, &cfg->pipe_xpcs_phy_ready, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_phymode_sel, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_rate_sel, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_sel_qsgmii, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->qsgmii_mode_set, true);
		break;

	default:
		dev_err(priv->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	rate = clk_get_rate(priv->refclk);

	switch (rate) {
	case REF_CLOCK_24MHz:
		if (priv->type == PHY_TYPE_USB3 || priv->type == PHY_TYPE_SATA) {
			/* Set ssc_cnt[9:0]=0101111101 & 31.5KHz. */
			val = PHYREG15_SSC_CNT_VALUE << PHYREG15_SSC_CNT_SHIFT;
			rockchip_combphy_updatel(priv, PHYREG15_SSC_CNT_MASK,
						 val, PHYREG15);

			writel(PHYREG16_SSC_CNT_VALUE, priv->mmio + PHYREG16);
		}
		break;

	case REF_CLOCK_25MHz:
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_clk_25m, true);
		break;

	case REF_CLOCK_100MHz:
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_clk_100m, true);
		if (priv->type == PHY_TYPE_PCIE) {
			/* PLL KVCO  fine tuning. */
			val = PHYREG33_PLL_KVCO_VALUE << PHYREG33_PLL_KVCO_SHIFT;
			rockchip_combphy_updatel(priv, PHYREG33_PLL_KVCO_MASK,
						 val, PHYREG33);

			/* Enable controlling random jitter. */
			writel(PHYREG12_PLL_LPF_ADJ_VALUE, priv->mmio + PHYREG12);

			val = PHYREG6_PLL_DIV_2 << PHYREG6_PLL_DIV_SHIFT;
			rockchip_combphy_updatel(priv, PHYREG6_PLL_DIV_MASK,
						 val, PHYREG6);

			writel(PHYREG18_PLL_LOOP, priv->mmio + PHYREG18);
			writel(PHYREG11_SU_TRIM_0_7, priv->mmio + PHYREG11);
		} else if (priv->type == PHY_TYPE_SATA) {
			/* downward spread spectrum +500ppm */
			val = PHYREG32_SSC_DOWNWARD << PHYREG32_SSC_DIR_SHIFT;
			val |= PHYREG32_SSC_OFFSET_500PPM << PHYREG32_SSC_OFFSET_SHIFT;
			rockchip_combphy_updatel(priv, PHYREG32_SSC_MASK, val, PHYREG32);
		}
		break;

	default:
		dev_err(priv->dev, "unsupported rate: %lu\n", rate);
		return -EINVAL;
	}

	if (priv->ext_refclk) {
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_clk_ext, true);
		if (priv->type == PHY_TYPE_PCIE && rate == REF_CLOCK_100MHz) {
			val = PHYREG13_RESISTER_HIGH_Z << PHYREG13_RESISTER_SHIFT;
			val |= PHYREG13_CKRCV_AMP0;
			rockchip_combphy_updatel(priv, PHYREG13_RESISTER_MASK, val, PHYREG13);

			val = readl(priv->mmio + PHYREG14);
			val |= PHYREG14_CKRCV_AMP1;
			writel(val, priv->mmio + PHYREG14);
		}
	}

	if (priv->enable_ssc) {
		val = readl(priv->mmio + PHYREG8);
		val |= PHYREG8_SSC_EN;
		writel(val, priv->mmio + PHYREG8);
	}

	return 0;
}

static const struct rockchip_combphy_grfcfg rk3568_combphy_grfcfgs = {
	/* pipe-phy-grf */
	.pcie_mode_set		= { 0x0000, 5, 0, 0x00, 0x11 },
	.usb_mode_set		= { 0x0000, 5, 0, 0x00, 0x04 },
	.sgmii_mode_set		= { 0x0000, 5, 0, 0x00, 0x01 },
	.qsgmii_mode_set	= { 0x0000, 5, 0, 0x00, 0x21 },
	.pipe_rxterm_set	= { 0x0000, 12, 12, 0x00, 0x01 },
	.pipe_txelec_set	= { 0x0004, 1, 1, 0x00, 0x01 },
	.pipe_txcomp_set	= { 0x0004, 4, 4, 0x00, 0x01 },
	.pipe_clk_25m		= { 0x0004, 14, 13, 0x00, 0x01 },
	.pipe_clk_100m		= { 0x0004, 14, 13, 0x00, 0x02 },
	.pipe_phymode_sel	= { 0x0008, 1, 1, 0x00, 0x01 },
	.pipe_rate_sel		= { 0x0008, 2, 2, 0x00, 0x01 },
	.pipe_rxterm_sel	= { 0x0008, 8, 8, 0x00, 0x01 },
	.pipe_txelec_sel	= { 0x0008, 12, 12, 0x00, 0x01 },
	.pipe_txcomp_sel	= { 0x0008, 15, 15, 0x00, 0x01 },
	.pipe_clk_ext		= { 0x000c, 9, 8, 0x02, 0x01 },
	.pipe_sel_usb		= { 0x000c, 14, 13, 0x00, 0x01 },
	.pipe_sel_qsgmii	= { 0x000c, 15, 13, 0x00, 0x07 },
	.pipe_phy_status	= { 0x0034, 6, 6, 0x01, 0x00 },
	.con0_for_pcie		= { 0x0000, 15, 0, 0x00, 0x1000 },
	.con1_for_pcie		= { 0x0004, 15, 0, 0x00, 0x0000 },
	.con2_for_pcie		= { 0x0008, 15, 0, 0x00, 0x0101 },
	.con3_for_pcie		= { 0x000c, 15, 0, 0x00, 0x0200 },
	.con0_for_sata		= { 0x0000, 15, 0, 0x00, 0x0119 },
	.con1_for_sata		= { 0x0004, 15, 0, 0x00, 0x0040 },
	.con2_for_sata		= { 0x0008, 15, 0, 0x00, 0x80c3 },
	.con3_for_sata		= { 0x000c, 15, 0, 0x00, 0x4407 },
	/* pipe-grf */
	.pipe_con0_for_sata	= { 0x0000, 15, 0, 0x00, 0x2220 },
	.pipe_xpcs_phy_ready	= { 0x0040, 2, 2, 0x00, 0x01 },
};

static const struct rockchip_combphy_cfg rk3568_combphy_cfgs = {
	.num_phys = 3,
	.phy_ids = {
		0xfe820000,
		0xfe830000,
		0xfe840000,
	},
	.grfcfg		= &rk3568_combphy_grfcfgs,
	.combphy_cfg	= rk3568_combphy_cfg,
};

static int rk3588_combphy_cfg(struct rockchip_combphy_priv *priv)
{
	const struct rockchip_combphy_grfcfg *cfg = priv->cfg->grfcfg;
	unsigned long rate;
	u32 val;

	switch (priv->type) {
	case PHY_TYPE_PCIE:
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con0_for_pcie, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con1_for_pcie, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con2_for_pcie, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con3_for_pcie, true);
		switch (priv->id) {
		case 1:
			rockchip_combphy_param_write(priv->pipe_grf, &cfg->pipe_pcie1l0_sel, true);
			break;
		case 2:
			rockchip_combphy_param_write(priv->pipe_grf, &cfg->pipe_pcie1l1_sel, true);
			break;
		}
		break;
	case PHY_TYPE_USB3:
		/* Set SSC downward spread spectrum */
		rockchip_combphy_updatel(priv, PHYREG32_SSC_MASK,
					 PHYREG32_SSC_DOWNWARD << PHYREG32_SSC_DIR_SHIFT,
					 PHYREG32);

		/* Enable adaptive CTLE for USB3.0 Rx. */
		val = readl(priv->mmio + PHYREG15);
		val |= PHYREG15_CTLE_EN;
		writel(val, priv->mmio + PHYREG15);

		/* Set PLL KVCO fine tuning signals. */
		rockchip_combphy_updatel(priv, PHYREG33_PLL_KVCO_MASK,
					 PHYREG33_PLL_KVCO_VALUE << PHYREG33_PLL_KVCO_SHIFT,
					 PHYREG33);

		/* Enable controlling random jitter. */
		writel(PHYREG12_PLL_LPF_ADJ_VALUE, priv->mmio + PHYREG12);

		/* Set PLL input clock divider 1/2. */
		rockchip_combphy_updatel(priv, PHYREG6_PLL_DIV_MASK,
					 PHYREG6_PLL_DIV_2 << PHYREG6_PLL_DIV_SHIFT,
					 PHYREG6);

		writel(PHYREG18_PLL_LOOP, priv->mmio + PHYREG18);
		writel(PHYREG11_SU_TRIM_0_7, priv->mmio + PHYREG11);

		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_txcomp_sel, false);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_txelec_sel, false);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->usb_mode_set, true);
		break;
	case PHY_TYPE_SATA:
		/* Enable adaptive CTLE for SATA Rx. */
		val = readl(priv->mmio + PHYREG15);
		val |= PHYREG15_CTLE_EN;
		writel(val, priv->mmio + PHYREG15);
		/*
		 * Set tx_rterm=50ohm and rx_rterm=44ohm for SATA.
		 * 0: 60ohm, 8: 50ohm 15: 44ohm (by step abort 1ohm)
		 */
		val = PHYREG7_TX_RTERM_50OHM << PHYREG7_TX_RTERM_SHIFT;
		val |= PHYREG7_RX_RTERM_44OHM << PHYREG7_RX_RTERM_SHIFT;
		writel(val, priv->mmio + PHYREG7);

		rockchip_combphy_param_write(priv->phy_grf, &cfg->con0_for_sata, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con1_for_sata, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con2_for_sata, true);
		rockchip_combphy_param_write(priv->phy_grf, &cfg->con3_for_sata, true);
		rockchip_combphy_param_write(priv->pipe_grf, &cfg->pipe_con0_for_sata, true);
		rockchip_combphy_param_write(priv->pipe_grf, &cfg->pipe_con1_for_sata, true);
		break;
	case PHY_TYPE_SGMII:
	case PHY_TYPE_QSGMII:
	default:
		dev_err(priv->dev, "incompatible PHY type\n");
		return -EINVAL;
	}

	rate = clk_get_rate(priv->refclk);

	switch (rate) {
	case REF_CLOCK_24MHz:
		if (priv->type == PHY_TYPE_USB3 || priv->type == PHY_TYPE_SATA) {
			/* Set ssc_cnt[9:0]=0101111101 & 31.5KHz. */
			val = PHYREG15_SSC_CNT_VALUE << PHYREG15_SSC_CNT_SHIFT;
			rockchip_combphy_updatel(priv, PHYREG15_SSC_CNT_MASK,
						 val, PHYREG15);

			writel(PHYREG16_SSC_CNT_VALUE, priv->mmio + PHYREG16);
		}
		break;

	case REF_CLOCK_25MHz:
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_clk_25m, true);
		break;
	case REF_CLOCK_100MHz:
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_clk_100m, true);
		if (priv->type == PHY_TYPE_PCIE) {
			/* PLL KVCO fine tuning. */
			val = 4 << PHYREG33_PLL_KVCO_SHIFT;
			rockchip_combphy_updatel(priv, PHYREG33_PLL_KVCO_MASK,
						 val, PHYREG33);

			/* Enable controlling random jitter. */
			writel(PHYREG12_PLL_LPF_ADJ_VALUE, priv->mmio + PHYREG12);

			/* Set up rx_trim: PLL LPF C1 85pf R1 1.25kohm */
			writel(PHYREG27_RX_TRIM_RK3588, priv->mmio + PHYREG27);

			/* Set up su_trim:  */
			writel(PHYREG11_SU_TRIM_0_7, priv->mmio + PHYREG11);
		} else if (priv->type == PHY_TYPE_SATA) {
			/* downward spread spectrum +500ppm */
			val = PHYREG32_SSC_DOWNWARD << PHYREG32_SSC_DIR_SHIFT;
			val |= PHYREG32_SSC_OFFSET_500PPM << PHYREG32_SSC_OFFSET_SHIFT;
			rockchip_combphy_updatel(priv, PHYREG32_SSC_MASK, val, PHYREG32);
		}
		break;
	default:
		dev_err(priv->dev, "Unsupported rate: %lu\n", rate);
		return -EINVAL;
	}

	if (priv->ext_refclk) {
		rockchip_combphy_param_write(priv->phy_grf, &cfg->pipe_clk_ext, true);
		if (priv->type == PHY_TYPE_PCIE && rate == REF_CLOCK_100MHz) {
			val = PHYREG13_RESISTER_HIGH_Z << PHYREG13_RESISTER_SHIFT;
			val |= PHYREG13_CKRCV_AMP0;
			rockchip_combphy_updatel(priv, PHYREG13_RESISTER_MASK, val, PHYREG13);

			val = readl(priv->mmio + PHYREG14);
			val |= PHYREG14_CKRCV_AMP1;
			writel(val, priv->mmio + PHYREG14);
		}
	}

	if (priv->enable_ssc) {
		val = readl(priv->mmio + PHYREG8);
		val |= PHYREG8_SSC_EN;
		writel(val, priv->mmio + PHYREG8);
	}

	return 0;
}

static const struct rockchip_combphy_grfcfg rk3588_combphy_grfcfgs = {
	/* pipe-phy-grf */
	.pcie_mode_set		= { 0x0000, 5, 0, 0x00, 0x11 },
	.usb_mode_set		= { 0x0000, 5, 0, 0x00, 0x04 },
	.pipe_rxterm_set	= { 0x0000, 12, 12, 0x00, 0x01 },
	.pipe_txelec_set	= { 0x0004, 1, 1, 0x00, 0x01 },
	.pipe_txcomp_set	= { 0x0004, 4, 4, 0x00, 0x01 },
	.pipe_clk_25m		= { 0x0004, 14, 13, 0x00, 0x01 },
	.pipe_clk_100m		= { 0x0004, 14, 13, 0x00, 0x02 },
	.pipe_rxterm_sel	= { 0x0008, 8, 8, 0x00, 0x01 },
	.pipe_txelec_sel	= { 0x0008, 12, 12, 0x00, 0x01 },
	.pipe_txcomp_sel	= { 0x0008, 15, 15, 0x00, 0x01 },
	.pipe_clk_ext		= { 0x000c, 9, 8, 0x02, 0x01 },
	.pipe_phy_status	= { 0x0034, 6, 6, 0x01, 0x00 },
	.con0_for_pcie		= { 0x0000, 15, 0, 0x00, 0x1000 },
	.con1_for_pcie		= { 0x0004, 15, 0, 0x00, 0x0000 },
	.con2_for_pcie		= { 0x0008, 15, 0, 0x00, 0x0101 },
	.con3_for_pcie		= { 0x000c, 15, 0, 0x00, 0x0200 },
	.con0_for_sata		= { 0x0000, 15, 0, 0x00, 0x0129 },
	.con1_for_sata		= { 0x0004, 15, 0, 0x00, 0x0000 },
	.con2_for_sata		= { 0x0008, 15, 0, 0x00, 0x80c1 },
	.con3_for_sata		= { 0x000c, 15, 0, 0x00, 0x0407 },
	/* pipe-grf */
	.pipe_con0_for_sata	= { 0x0000, 11, 5, 0x00, 0x22 },
	.pipe_con1_for_sata	= { 0x0000, 2, 0, 0x00, 0x2 },
	.pipe_pcie1l0_sel	= { 0x0100, 0, 0, 0x01, 0x0 },
	.pipe_pcie1l1_sel	= { 0x0100, 1, 1, 0x01, 0x0 },
};

static const struct rockchip_combphy_cfg rk3588_combphy_cfgs = {
	.num_phys = 3,
	.phy_ids = {
		0xfee00000,
		0xfee10000,
		0xfee20000,
	},
	.grfcfg		= &rk3588_combphy_grfcfgs,
	.combphy_cfg	= rk3588_combphy_cfg,
};

static const struct udevice_id rockchip_combphy_of_match[] = {
	{
		.compatible = "rockchip,rk3568-naneng-combphy",
		.data = &rk3568_combphy_cfgs,
	},
	{
		.compatible = "gedu,rk3588-naneng-combphy",
		.data = &rk3588_combphy_cfgs,
	},
	{ },
};

U_BOOT_DRIVER(rockchip_naneng_combphy_port) = {
	.name		= "rockchip_naneng_combphy_port",
	.id		= UCLASS_PHY,
	.ops		= &rockchip_combphy_ops,
};

U_BOOT_DRIVER(rockchip_naneng_combphy) = {
	.name		= "rockchip_naneng_combphy",
	.id		= UCLASS_PHY,
	.of_match	= rockchip_combphy_of_match,
	.probe		= rockchip_combphy_probe,
	.bind		= rockchip_combphy_bind,
	.priv_auto_alloc_size = sizeof(struct rockchip_combphy_priv),
};

#ifdef LINUX
MODULE_DEVICE_TABLE(of, rockchip_combphy_of_match);

static struct platform_driver rockchip_combphy_driver = {
	.probe	= rockchip_combphy_probe,
	.driver = {
		.name = "rockchip-naneng-combphy",
		.of_match_table = rockchip_combphy_of_match,
	},
};
module_platform_driver(rockchip_combphy_driver);

MODULE_DESCRIPTION("Rockchip NANENG COMBPHY driver");
MODULE_LICENSE("GPL v2");
#endif