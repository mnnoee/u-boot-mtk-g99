// SPDX-License-Identifier: GPL-2.0
/*
 * Mediatek "glue layer"
 *
 * Copyright (C) 2019-2021 by Mediatek
 * Based on the AllWinner SUNXI "glue layer" code.
 * Copyright (C) 2015 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2013 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of the Inventra Controller Driver for Linux.
 */
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/root.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/usb/musb.h>
#include <usb.h>
#include "linux-compat.h"
#include "musb_core.h"
#include "musb_uboot.h"

#define DBG_I(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

struct mtk_musb_config {
	struct musb_hdrc_config *config;
};

struct mtk_musb_glue {
	struct musb_host_data mdata;
	struct clk usbpllclk;
	struct clk usbmcuclk;
	struct clk usbclk;
	struct mtk_musb_config *cfg;
	struct device dev;
};

#define to_mtk_musb_glue(d)	container_of(d, struct mtk_musb_glue, dev)

/******************************************************************************
 * phy settings — MT6789 USB20 PHY
 *
 * Register bank at 0x11f40300+ (from stock LK binary analysis):
 *   0x318 — PHY control register 1
 *   0x31c — PHY control register 2
 *   0x320 — PHY control register 3
 *   0x368 — PHY analog control
 *   0x36c — PHY calibration (eye-vrt, eye-term, discth, rx_sqth)
 ******************************************************************************/
#define USB20_PHY_BASE			0x11f40000

/* Byte-access macros (for 0x000-0x0FF range) */
#define USBPHY_READ8(offset)	 \
	readb((void *)(USB20_PHY_BASE + (offset)))
#define USBPHY_WRITE8(offset, value)	\
	writeb(value, (void *)(USB20_PHY_BASE + (offset)))
#define USBPHY_SET8(offset, mask)	\
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) | (mask))
#define USBPHY_CLR8(offset, mask)	\
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) & (~(mask)))

/* 32-bit access macros (for 0x300+ range — required for MT6789) */
#define USBPHY_READ32(offset)	 \
	readl((void *)(USB20_PHY_BASE + (offset)))
#define USBPHY_WRITE32(offset, value)	\
	writel(value, (void *)(USB20_PHY_BASE + (offset)))

/* MT6789 PHY register offsets (from stock LK binary) */
#define PHY_REG_318		0x318
#define PHY_REG_31c		0x31c
#define PHY_REG_320		0x320
#define PHY_REG_368		0x368
#define PHY_REG_36c		0x36c

static void mt_usb_phy_poweron(void)
{
	u32 val;

	/* From stock LK FUN_0004a1f0 */
	val = USBPHY_READ32(PHY_REG_368);
	val &= 0xfbfbffff;
	USBPHY_WRITE32(PHY_REG_368, val);

	val = USBPHY_READ32(PHY_REG_36c);
	val &= 0xfffeffff;
	USBPHY_WRITE32(PHY_REG_36c, val);

	val = USBPHY_READ32(PHY_REG_320);
	val = (val & 0xfff8fcff) | 0x40000;
	USBPHY_WRITE32(PHY_REG_320, val);

	val = USBPHY_READ32(PHY_REG_318);
	val = (val & 0xff7fffff) | 0x100000;
	USBPHY_WRITE32(PHY_REG_318, val);

	mdelay(800);
}

static void mt_usb_phy_savecurrent(void)
{
	u32 val;

	/* From stock LK FUN_0004a240 */
	val = USBPHY_READ32(PHY_REG_318);
	val &= 0xff6fffff;
	USBPHY_WRITE32(PHY_REG_318, val);

	val = USBPHY_READ32(PHY_REG_36c);
	val &= 0xfffeffff;
	USBPHY_WRITE32(PHY_REG_36c, val);

	val = USBPHY_READ32(PHY_REG_320);
	val &= 0xfffffcff;
	USBPHY_WRITE32(PHY_REG_320, val);

	val = USBPHY_READ32(PHY_REG_368);
	val = (val & 0xfb41c303) | 0xbe00dc;
	USBPHY_WRITE32(PHY_REG_368, val);

	mdelay(800);

	val = USBPHY_READ32(PHY_REG_368);
	val &= 0xfffffff7;
	USBPHY_WRITE32(PHY_REG_368, val);

	mdelay(1);
}

static void mt_usb_phy_recover(void)
{
	u32 val;

	/* From stock LK FUN_0004a2bc */
	val = USBPHY_READ32(PHY_REG_368);
	val &= 0xfb41c30b;
	USBPHY_WRITE32(PHY_REG_368, val);

	val = USBPHY_READ32(PHY_REG_36c);
	val &= 0xfffeffff;
	USBPHY_WRITE32(PHY_REG_36c, val);

	val = USBPHY_READ32(PHY_REG_31c);
	val &= 0xffffefff;
	USBPHY_WRITE32(PHY_REG_31c, val);

	val = USBPHY_READ32(PHY_REG_320);
	val &= 0xfffffcff;
	USBPHY_WRITE32(PHY_REG_320, val);

	val = USBPHY_READ32(PHY_REG_318);
	val = (val & 0xff7fffff) | 0x100000;
	USBPHY_WRITE32(PHY_REG_318, val);

	mdelay(800);

	/* Calibration: eye-vrt, eye-term, discth, rx_sqth */
	val = USBPHY_READ32(PHY_REG_36c);
	val = (val & 0xffffffcf) | 0x3f2f;
	USBPHY_WRITE32(PHY_REG_36c, val);

	printf("USB PHY: reg318=%08x reg320=%08x reg368=%08x reg36c=%08x\n",
	       USBPHY_READ32(PHY_REG_318), USBPHY_READ32(PHY_REG_320),
	       USBPHY_READ32(PHY_REG_368), USBPHY_READ32(PHY_REG_36c));
}

/******************************************************************************
 * MUSB Glue code
 ******************************************************************************/

static irqreturn_t mtk_musb_interrupt(int irq, void *__hci)
{
	struct musb		*musb = __hci;
	irqreturn_t		retval = IRQ_NONE;

	/* read and flush interrupts */
	musb->int_usb = musb_readb(musb->mregs, MUSB_INTRUSB);
//	last_int_usb = musb->int_usb;
	if (musb->int_usb)
		musb_writeb(musb->mregs, MUSB_INTRUSB, musb->int_usb);
	musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX);
	if (musb->int_tx)
		musb_writew(musb->mregs, MUSB_INTRTX, musb->int_tx);
	musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX);
	if (musb->int_rx)
		musb_writew(musb->mregs, MUSB_INTRRX, musb->int_rx);

	if (musb->int_usb || musb->int_tx || musb->int_rx)
		retval |= musb_interrupt(musb);

	return retval;
}

/* musb_core does not call enable / disable in a balanced manner <sigh> */
static bool enabled;

static int mtk_musb_enable(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);

	DBG_I("%s():\n", __func__);

	musb_ep_select(musb->mregs, 0);
	musb_writeb(musb->mregs, MUSB_FADDR, 0);

	if (enabled)
		return 0;

	mt_usb_phy_recover();

	enabled = true;

	return 0;
}

static void mtk_musb_disable(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);
	int ret;

	DBG_I("%s():\n", __func__);

	if (!enabled)
		return;

	mt_usb_phy_savecurrent();

	enabled = false;
}

static int mtk_musb_init(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);
	int ret;

	DBG_I("%s():\n", __func__);

	ret = clk_enable(&glue->usbpllclk);
	if (ret) {
		dev_err(musb->controller, "failed to enable usbpll clock\n");
		return ret;
	}
	ret = clk_enable(&glue->usbmcuclk);
	if (ret) {
		dev_err(musb->controller, "failed to enable usbmcu clock\n");
		return ret;
	}
	ret = clk_enable(&glue->usbclk);
	if (ret) {
		dev_err(musb->controller, "failed to enable usb clock\n");
		return ret;
	}

	musb->isr = mtk_musb_interrupt;

	return 0;
}

static int mtk_musb_exit(struct musb *musb)
{
	struct mtk_musb_glue *glue = to_mtk_musb_glue(musb->controller);

	clk_disable(&glue->usbclk);
	clk_disable(&glue->usbmcuclk);
	clk_disable(&glue->usbpllclk);

	return 0;
}

static const struct musb_platform_ops mtk_musb_ops = {
	.init		= mtk_musb_init,
	.exit		= mtk_musb_exit,
	.enable		= mtk_musb_enable,
	.disable	= mtk_musb_disable,
};

/* MTK OTG supports up to 7 endpoints */
#define MTK_MUSB_MAX_EP_NUM		8
#define MTK_MUSB_RAM_BITS		16

static struct musb_fifo_cfg mtk_musb_mode_cfg[] = {
	MUSB_EP_FIFO_SINGLE(1, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(1, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(2, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(3, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(4, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(4, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(5, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(5, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(6, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(6, FIFO_RX, 512),
	MUSB_EP_FIFO_SINGLE(7, FIFO_TX, 512),
	MUSB_EP_FIFO_SINGLE(7, FIFO_RX, 512),
};

static struct musb_hdrc_config musb_config = {
	.fifo_cfg       = mtk_musb_mode_cfg,
	.fifo_cfg_size  = ARRAY_SIZE(mtk_musb_mode_cfg),
	.multipoint	= true,
	.dyn_fifo	= true,
	.num_eps	= MTK_MUSB_MAX_EP_NUM,
	.ram_bits	= MTK_MUSB_RAM_BITS,
};

static int musb_usb_probe(struct udevice *dev)
{
	struct mtk_musb_glue *glue;
	struct musb_host_data *host;
	struct musb_hdrc_platform_data pdata;
	void *base;
	int ret;

	glue = dev_get_priv(dev);
	host = &glue->mdata;

	base = dev_read_addr_ptr(dev);
	if (!base) {
		/* Fallback: use physical address directly if map_sysmem fails */
		fdt_addr_t addr = dev_read_addr(dev);
		if (addr != FDT_ADDR_T_NONE)
			base = (void *)(uintptr_t)addr;
	}

#ifdef CONFIG_USB_MUSB_HOST
	struct usb_bus_priv *priv = dev_get_uclass_priv(dev);
#endif

	if (!base)
		return -EINVAL;

	glue->cfg = (struct mtk_musb_config *)dev_get_driver_data(dev);
	if (!glue->cfg)
		return -EINVAL;

	ret = clk_get_by_name(dev, "usbpll", &glue->usbpllclk);
	if (ret) {
		dev_err(dev, "failed to get usbpll clock\n");
		return ret;
	}
	ret = clk_get_by_name(dev, "usbmcu", &glue->usbmcuclk);
	if (ret) {
		dev_err(dev, "failed to get usbmcu clock\n");
		return ret;
	}
	ret = clk_get_by_name(dev, "usb", &glue->usbclk);
	if (ret) {
		dev_err(dev, "failed to get usb clock\n");
		return ret;
	}

	memset(&pdata, 0, sizeof(pdata));
	pdata.power = (u8)400;
	pdata.platform_ops = &mtk_musb_ops;
	pdata.config = glue->cfg->config;

#ifdef CONFIG_USB_MUSB_HOST
	priv->desc_before_addr = true;

	pdata.mode = MUSB_HOST;
	host->host = musb_init_controller(&pdata, &glue->dev, base);
	if (!host->host)
		return -EIO;

	ret = musb_lowlevel_init(host);
	if (!ret)
		printf("MTK MUSB OTG (Host)\n");
#else
	pdata.mode = MUSB_PERIPHERAL;
	host->host = musb_register(&pdata, &glue->dev, base);
	if (!host->host)
		return -EIO;

	printf("MTK MUSB OTG (Peripheral)\n");
#endif

	mt_usb_phy_poweron();

	return ret;
}

static int musb_usb_remove(struct udevice *dev)
{
	struct mtk_musb_glue *glue = dev_get_priv(dev);
	struct musb_host_data *host = &glue->mdata;

	musb_stop(host->host);
	free(host->host);
	host->host = NULL;

	return 0;
}

/*
 * Board USB init - triggers DM probe of MUSB device so that
 * musb_register() sets the static 'gadget' variable used by
 * the old-style usb_gadget_register_driver path.
 */
int board_usb_init(int index, enum usb_init_type init)
{
	struct udevice *phy_dev;
	struct udevice *dev;
	int ret;

	if (init != USB_INIT_DEVICE)
		return -ENODEV;

	/* Probe PHY first - it's a child of usb0@11200000 */
	ret = uclass_get_device_by_name(UCLASS_PHY, "t-phy@11f40000", &phy_dev);
	if (ret)
		printf("MUSB: PHY probe failed: %d (non-fatal)\n", ret);
	else
		printf("MUSB: PHY probed OK\n");

	ret = uclass_get_device_by_seq(UCLASS_USB_GADGET_GENERIC, index, &dev);
	if (ret) {
		printf("MUSB: board_usb_init: uclass_get_device failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct mtk_musb_config mt8518_cfg = {
	.config = &musb_config,
};

static const struct udevice_id mtk_musb_ids[] = {
	{ .compatible = "mediatek,mt8518-musb",
	  .data = (ulong)&mt8518_cfg },
	{ }
};

U_BOOT_DRIVER(mtk_musb) = {
	.name		= "mtk_musb",
#ifdef CONFIG_USB_MUSB_HOST
	.id		= UCLASS_USB,
#else
	.id		= UCLASS_USB_GADGET_GENERIC,
#endif
	.of_match	= mtk_musb_ids,
	.probe		= musb_usb_probe,
	.remove		= musb_usb_remove,
#ifdef CONFIG_USB_MUSB_HOST
	.ops		= &musb_usb_ops,
#endif
	.plat_auto	= sizeof(struct usb_plat),
	.priv_auto	= sizeof(struct mtk_musb_glue),
};
