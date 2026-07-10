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
 * Matrix values from register dumps (register 0x1001000C = KP_MEM3):
 *   Volume Down: 0xbfdf (bits 5,13 low = pressed)
 *   Volume Up:   0xdfef (bits 4,12 low = pressed)
 */

#include <button.h>
#include <dm.h>
#include <linux/io.h>
#include <linux/bitops.h>

/* MediaTek KP registers (from mt6779-keypad.c) */
#define KP_MEM1		0x0004	/* Matrix row 1 */
#define KP_MEM2		0x0008	/* Matrix row 2 */
#define KP_MEM3		0x000c	/* Matrix row 3 */
#define KP_MEM4		0x0010	/* Matrix row 4 */
#define KP_MEM5		0x0014	/* Matrix row 5 (only 8 bits) */

#define KP_NUM_MEMS	5
#define KP_NUM_BITS	136	/* 4*32 + 8 */

/* Matrix values for each button (active low, from register 0x1001000C) */
#define KP_VOLUME_DOWN	0xbfdf
#define KP_VOLUME_UP	0xdfef

struct button_mtk_kp_priv {
	void __iomem *base;
	int linux_code;
	u32 matrix_val;		/* Expected matrix value for this button */
	u32 matrix_reg;		/* Register offset containing this button's bits */
};

static int button_mtk_kp_get_state(struct udevice *dev)
{
	struct button_mtk_kp_priv *priv = dev_get_priv(dev);
	u32 val;

	if (!priv || !priv->base)
		return -ENODATA;

	/* Read the matrix register that contains this button */
	val = readl(priv->base + priv->matrix_reg);

	/* Check if our specific button bits are active (0 = pressed) */
	if ((val & priv->matrix_val) != priv->matrix_val)
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

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->base = devm_ioremap(dev, addr, 0x100);
	if (!priv->base)
		return -ENOMEM;

	/* Read matrix value and register offset from DTS */
	priv->matrix_val = dev_read_u32_default(dev, "mediatek,matrix-val", 0);
	priv->matrix_reg = dev_read_u32_default(dev, "mediatek,matrix-reg", KP_MEM3);

	debug("MTK KP: base=%p, code=%d, reg=0x%x, matrix=0x%04x\n",
	      priv->base, priv->linux_code, priv->matrix_reg, priv->matrix_val);

	return 0;
}

static int button_mtk_kp_ofdata_to_plat(struct udevice *dev)
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
	{ }
};

U_BOOT_DRIVER(button_mtk_kp) = {
	.name		= "button_mtk_kp",
	.id		= UCLASS_BUTTON,
	.of_match	= button_mtk_kp_ids,
	.ops		= &button_mtk_kp_ops,
	.priv_auto	= sizeof(struct button_mtk_kp_priv),
	.probe		= button_mtk_kp_probe,
	.ofdata_to_plat = button_mtk_kp_ofdata_to_plat,
};
