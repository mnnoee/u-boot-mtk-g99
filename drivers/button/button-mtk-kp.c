// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek Keypad Controller button driver for MT6789
 *
 * Based on Linux kernel driver: drivers/input/keyboard/mt6779-keypad.c
 * Register layout:
 *   0x0004-0x0014: KP_MEM1-KP_MEM5 (matrix data, 5 x 32-bit)
 *   Bits [15:0] of each register: 0=pressed, 1=released (active low)
 *   Total: 136 key positions (4*32 + 8)
 *
 * Also enables essential INFRACFG_AO gate clocks for boot peripherals.
 * Clock gate definitions from: drivers/clk/mediatek/clk-mt6789-infra_ao.c
 */

#include <button.h>
#include <dm.h>
#include <linux/io.h>
#include <linux/bitops.h>

/* MediaTek KP registers (from mt6779-keypad.c) */
#define KP_MEM1		0x0004
#define KP_MEM2		0x0008
#define KP_MEM3		0x000c
#define KP_MEM4		0x0010
#define KP_MEM5		0x0014

/*
 * INFRACFG_AO clock gate registers (from clk-mt6789-infra_ao.c)
 * Base: 0x10001000
 *
 * Gate register groups:
 *   IFRAO0: SET=0x80, CLR=0x84, STA=0x90
 *   IFRAO1: SET=0x88, CLR=0x8C, STA=0x94
 *   IFRAO2: SET=0xA4, CLR=0xA8, STA=0xAC
 *   IFRAO3: SET=0xC0, CLR=0xC4, STA=0xC8
 *   IFRAO4: SET=0xE0, CLR=0xE4, STA=0xE8
 *   IFRAO5: SET=0xD0, CLR=0xD4, STA=0xD8
 */
#define INFRACFG_AO_BASE		0x10001000

/* IFRAO gate SET registers */
#define IFRAO0_SET_OFS			0x80
#define IFRAO1_SET_OFS			0x88
#define IFRAO2_SET_OFS			0xA4
#define IFRAO4_SET_OFS			0xE0

/* IFRAO0 gate bits (from clk-mt6789-infra_ao.c) */
#define CLK_IFRAO_I2C_PSEUDO	BIT(11)
#define CLK_IFRAO_UART0	BIT(22)

/* IFRAO1 gate bits */
#define CLK_IFRAO_SPI0		BIT(1)
#define CLK_IFRAO_MSDC0	BIT(2)
#define CLK_IFRAO_MSDC0_SRC	BIT(6)

/* IFRAO2 gate bits */
#define CLK_IFRAO_SSUSB		BIT(1)
#define CLK_IFRAO_UFS		BIT(28)
#define CLK_IFRAO_UFS_AES	BIT(29)

/* IFRAO4 gate bits */
#define CLK_IFRAO_KP		BIT(2)

struct button_mtk_kp_priv {
	void __iomem *base;
	int linux_code;
	u32 matrix_val;
	u32 matrix_reg;
};

static void mt6789_enable_essential_clocks(void)
{
	u32 val;

	/* IFRAO0: UART0, I2C */
	val = readl(INFRACFG_AO_BASE + IFRAO0_SET_OFS);
	val |= CLK_IFRAO_I2C_PSEUDO | CLK_IFRAO_UART0;
	writel(val, INFRACFG_AO_BASE + IFRAO0_SET_OFS);

	/* IFRAO1: SPI0, MSDC0, MSDC0_SRC */
	val = readl(INFRACFG_AO_BASE + IFRAO1_SET_OFS);
	val |= CLK_IFRAO_SPI0 | CLK_IFRAO_MSDC0 | CLK_IFRAO_MSDC0_SRC;
	writel(val, INFRACFG_AO_BASE + IFRAO1_SET_OFS);

	/* IFRAO2: SSUSB (USB), UFS, UFS_AES */
	val = readl(INFRACFG_AO_BASE + IFRAO2_SET_OFS);
	val |= CLK_IFRAO_SSUSB | CLK_IFRAO_UFS | CLK_IFRAO_UFS_AES;
	writel(val, INFRACFG_AO_BASE + IFRAO2_SET_OFS);

	/* IFRAO4: Keypad */
	val = readl(INFRACFG_AO_BASE + IFRAO4_SET_OFS);
	val |= CLK_IFRAO_KP;
	writel(val, INFRACFG_AO_BASE + IFRAO4_SET_OFS);

	debug("MT6789 clocks: IFRAO0=0x%08x IFRAO1=0x%08x IFRAO2=0x%08x IFRAO4=0x%08x\n",
	      readl(INFRACFG_AO_BASE + IFRAO0_SET_OFS),
	      readl(INFRACFG_AO_BASE + IFRAO1_SET_OFS),
	      readl(INFRACFG_AO_BASE + IFRAO2_SET_OFS),
	      readl(INFRACFG_AO_BASE + IFRAO4_SET_OFS));
}

static enum button_state_t button_mtk_kp_get_state(struct udevice *dev)
{
	struct button_mtk_kp_priv *priv = dev_get_priv(dev);
	u32 val;

	if (!priv || !priv->base)
		return BUTTON_OFF;

	val = readl(priv->base + priv->matrix_reg);

	/* Active-low: bits go LOW when button is pressed */
	if (val == priv->matrix_val)
		return BUTTON_ON;

	return BUTTON_OFF;
}

static int button_mtk_kp_get_code(struct udevice *dev)
{
	struct button_mtk_kp_priv *priv = dev_get_priv(dev);

	if (!priv)
		return -ENODATA;

	return priv->linux_code;
}

static int button_mtk_kp_probe(struct udevice *dev)
{
	struct button_mtk_kp_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	printf("KP: probe start\n");

	addr = dev_read_addr(dev);
	printf("KP: addr=0x%llx\n", (unsigned long long)addr);
	if (addr == FDT_ADDR_T_NONE) {
		printf("KP: bad addr\n");
		return -EINVAL;
	}

	printf("KP: ioremap...\n");
	priv->base = devm_ioremap(dev, addr, 0x100);
	printf("KP: base=%p\n", priv->base);
	if (!priv->base)
		return -ENOMEM;

	printf("KP: reading props...\n");
	priv->matrix_val = dev_read_u32_default(dev, "mediatek,matrix-val", 0);
	priv->matrix_reg = dev_read_u32_default(dev, "mediatek,matrix-reg", KP_MEM3);
	printf("KP: code=%d reg=0x%x val=0x%04x\n",
	      priv->linux_code, priv->matrix_reg, priv->matrix_val);

	return 0;
}

static int button_mtk_kp_bind(struct udevice *dev)
{
	struct button_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	const char *label;

	label = dev_read_string(dev, "label");
	if (!label) {
		debug("MTK KP: node %s has no label\n", dev->name);
		return -EINVAL;
	}

	uc_plat->label = label;
	debug("MTK KP: bound button '%s'\n", label);

	return 0;
}

static int button_mtk_kp_of_to_plat(struct udevice *dev)
{
	struct button_mtk_kp_priv *priv = dev_get_priv(dev);

	priv->linux_code = dev_read_u32_default(dev, "linux,code", 0);

	return 0;
}

static const struct button_ops button_mtk_kp_ops = {
	.get_state = button_mtk_kp_get_state,
	.get_code = button_mtk_kp_get_code,
};

static const struct udevice_id button_mtk_kp_ids[] = {
	{ .compatible = "mediatek,mt6779-keypad" },
	{ .compatible = "mediatek,kp" },
	{ }
};

U_BOOT_DRIVER(button_mtk_kp) = {
	.name		= "button_mtk_kp",
	.id		= UCLASS_BUTTON,
	.of_match	= button_mtk_kp_ids,
	.ops		= &button_mtk_kp_ops,
	.priv_auto	= sizeof(struct button_mtk_kp_priv),
	.bind		= button_mtk_kp_bind,
	.probe		= button_mtk_kp_probe,
	.of_to_plat	= button_mtk_kp_of_to_plat,
};
