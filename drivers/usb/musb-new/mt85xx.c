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
 * phy settings
 ******************************************************************************/
#define USB20_PHY_BASE			0x11f40000
#define USBPHY_READ8(offset)	 \
	readb((void *)(USB20_PHY_BASE + (offset)))
#define USBPHY_WRITE8(offset, value)	\
	writeb(value, (void *)(USB20_PHY_BASE + (offset)))
#define USBPHY_SET8(offset, mask)	\
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) | (mask))
#define USBPHY_CLR8(offset, mask)	\
	USBPHY_WRITE8(offset, (USBPHY_READ8(offset)) & (~(mask)))

/*
 * MT6789 USB PHY calibration values (from vendor firmware DTB)
 * These match mediatek,eye-vrt, mediatek,eye-term, etc. properties.
 *
 * Register map (relative to USB20_PHY_BASE 0x11f40000):
 *   0x004 USBPHYACR1 [14:12] = VRT_SEL (eye-vrt)
 *                          [10:8] = TERM_SEL (eye-term)
 *   0x018 USBPHYACR6 [ 7:4]  = DISCTH (discth)
 *                      [ 3:0]  = SQTH (rx_sqth)
 */
#define PHY_EYE_VRT		4
#define PHY_EYE_TERM		4
#define PHY_DISCTH		7
#define PHY_RX_SQTH		5

static void mt_usb_phy_calibrate(void)
{
	u8 reg;

	/* USBPHYACR1 (0x004): set eye-vrt and eye-term */
	reg = USBPHY_READ8(0x005);
	reg &= ~0x77;		/* clear bits [6:4] and [2:0] */
	reg |= (PHY_EYE_VRT << 4) | PHY_EYE_TERM;
	USBPHY_WRITE8(0x005, reg);

	/* USBPHYACR6 (0x018): set discth and rx_sqth */
	reg = USBPHY_READ8(0x18);
	reg &= ~0xff;		/* clear all bits */
	reg |= (PHY_DISCTH << 4) | PHY_RX_SQTH;
	USBPHY_WRITE8(0x18, reg);
}

static void mt_usb_phy_poweron(void)
{
	/* Apply PHY calibration values (eye-vrt, eye-term, discth, rx_sqth) */
	mt_usb_phy_calibrate();

	/*
	 * switch to USB function.
	 * (system register, force ip into usb mode).
	 */
	USBPHY_CLR8(0x6b, 0x04);
	USBPHY_CLR8(0x6e, 0x01);
	USBPHY_CLR8(0x21, 0x03);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USBPHY_SET8(0x22, 0x04);
	USBPHY_CLR8(0x1a, 0x80);

	/* RG_USB20_DP_100K_EN = 1'b0 */
	USBPHY_CLR8(0x22, 0x03);

	/*OTG enable*/
	USBPHY_SET8(0x20, 0x10);
	/* release force suspendm */
	USBPHY_CLR8(0x6a, 0x04);

	mdelay(800);

	/* force enter device mode */
	USBPHY_CLR8(0x6c, 0x10);
	USBPHY_SET8(0x6c, 0x2E);
	USBPHY_SET8(0x6d, 0x3E);
}

static void mt_usb_phy_savecurrent(void)
{
	/*
	 * switch to USB function.
	 * (system register, force ip into usb mode).
	 */
	USBPHY_CLR8(0x6b, 0x04);
	USBPHY_CLR8(0x6e, 0x01);
	USBPHY_CLR8(0x21, 0x03);

	/* release force suspendm */
	USBPHY_CLR8(0x6a, 0x04);
	USBPHY_SET8(0x68, 0x04);
	/* RG_DPPULLDOWN./RG_DMPULLDOWN. */
	USBPHY_SET8(0x68, 0xc0);
	/* RG_XCVRSEL[1:0] = 2'b01 */
	USBPHY_CLR8(0x68, 0x30);
	USBPHY_SET8(0x68, 0x10);
	/* RG_TERMSEL = 1'b1 */
	USBPHY_SET8(0x68, 0x04);
	/* RG_DATAIN[3:0] = 4'b0000 */
	USBPHY_CLR8(0x69, 0x3c);

	/*
	 * force_dp_pulldown, force_dm_pulldown,
	 * force_xcversel, force_termsel.
	 */
	USBPHY_SET8(0x6a, 0xba);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USBPHY_CLR8(0x1a, 0x80);
	/* RG_USB20_OTG_VBUSSCMP_EN = 1'b0 */
	USBPHY_CLR8(0x1a, 0x10);

	mdelay(800);

	USBPHY_CLR8(0x6a, 0x04);
	/* rg_usb20_pll_stable = 1 */
	//USBPHY_SET8(0x63, 0x02);

	mdelay(1);

	/* force suspendm = 1 */
	//USBPHY_SET8(0x6a, 0x04);
}

static void mt_usb_phy_recover(void)
{
	/* clean PUPD_BIST_EN */
	/* PUPD_BIST_EN = 1'b0 */
	/* PMIC will use it to detect charger type */
	USBPHY_CLR8(0x1d, 0x10);

	/* force_uart_en = 1'b0 */
	USBPHY_CLR8(0x6b, 0x04);
	/* RG_UART_EN = 1'b0 */
	USBPHY_CLR8(0x6e, 0x01);
	/* force_uart_en = 1'b0 */
	USBPHY_CLR8(0x6a, 0x04);

	USBPHY_CLR8(0x21, 0x03);
	USBPHY_CLR8(0x68, 0xf4);

	/* RG_DATAIN[3:0] = 4'b0000 */
	USBPHY_CLR8(0x69, 0x3c);

	USBPHY_CLR8(0x6a, 0xba);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	USBPHY_CLR8(0x1a, 0x80);
	/* RG_USB20_OTG_VBUSSCMP_EN = 1'b1 */
	USBPHY_SET8(0x1a, 0x10);

	/*OTG enable*/
	USBPHY_SET8(0x20, 0x10);
	/* release force suspendm */
	USBPHY_CLR8(0x6a, 0x04);

	/* Apply PHY calibration values */
	mt_usb_phy_calibrate();

	mdelay(800);

	/* force enter device mode */
	USBPHY_CLR8(0x6c, 0x10);
	USBPHY_SET8(0x6c, 0x2E);
	USBPHY_SET8(0x6d, 0x3E);

	/* enable VRT internal R architecture */
	/* RG_USB20_INTR_EN = 1'b1 */
	USBPHY_SET8(0x00, 0x20);
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
