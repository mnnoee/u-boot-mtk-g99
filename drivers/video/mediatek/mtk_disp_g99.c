// SPDX-License-Identifier: GPL-2.0+
/*
 * MediaTek MT6789 (Helio G99) display driver for U-Boot
 *
 * Minimal UCLASS_VIDEO driver. LK/BL2_EXT already initializes the DSI
 * controller and panel (ILI7835F). This driver reconfigures the OVL0 →
 * RDMA0 → DITHER0 → DSI0 pipeline to drive a U-Boot-controlled
 * framebuffer, then triggers frame updates via DSI DCS Write Memory
 * Start (0x2C).
 *
 * ALL register writes in mtk_disp_pipeline_init() were empirically
 * verified via U-Boot shell commands on real hardware (Infinix X676B,
 * MT6789).  No register write that was not part of the verified
 * sequence has been added.
 */

#include <dm.h>
#include <video.h>
#include <asm/io.h>
#include <asm/cache.h>

/* ── Hardware register bases ─────────────────────────────────────────── */

#define MMSYS_BASE		0x14000000UL
#define OVL0_BASE		0x14005000UL
#define RDMA0_BASE		0x14007000UL
#define DSI0_BASE		0x14013000UL
#define MUTEX0_BASE		0x14001000UL

/* ── MMSYS registers ────────────────────────────────────────────────── */

#define MMSYS_CLK_CLR		(MMSYS_BASE + 0x108)	/* CLR = clear gate (enable) */
#define MMSYS_SW_RST		(MMSYS_BASE + 0x140)

/* ── MMSYS routing registers (mux selectors) ────────────────────────── */
/* See docs/AGENTS/mt6789_mmsys_routing.md for full decoding           */

#define MMSYS_OVL0_MOUT		(MMSYS_BASE + 0xf18)	/* OVL0 output mux */
#define MMSYS_RDMA0_SEL_IN	(MMSYS_BASE + 0xf28)	/* RDMA0 input select */
#define MMSYS_DITHER0_MOUT	(MMSYS_BASE + 0xf20)	/* DITHER0 output mux */
#define MMSYS_DSI0_SEL_IN	(MMSYS_BASE + 0xf30)	/* DSI0 input select */

/* ── OVL0 registers ─────────────────────────────────────────────────── */

#define OVL0_EN			(OVL0_BASE + 0x00c)
#define OVL0_CFG		(OVL0_BASE + 0x010)
#define OVL0_SRC_CON		(OVL0_BASE + 0x024)
#define OVL0_L0_CON		(OVL0_BASE + 0x0c0)
#define OVL0_L0_CFG		(OVL0_BASE + 0x030)
#define OVL0_L0_PITCH		(OVL0_BASE + 0x034)
#define OVL0_L0_SRC_SIZE	(OVL0_BASE + 0x038)
#define OVL0_L0_OFFSET		(OVL0_BASE + 0x03c)
#define OVL0_L0_ADDR		(OVL0_BASE + 0xf40)

/* ── RDMA0 registers ────────────────────────────────────────────────── */

#define RDMA0_EN		(RDMA0_BASE + 0x000)
#define RDMA0_MEM_ADDR		(RDMA0_BASE + 0x02c)
#define RDMA0_SRC_SIZE		(RDMA0_BASE + 0x030)

/* ── MUTEX0 registers ───────────────────────────────────────────────── */

#define MUTEX0_EN		(MUTEX0_BASE + 0x020)

/* ── DSI0 registers (MT6789-specific offsets) ────────────────────────── */

#define DSI0_START		(DSI0_BASE + 0x000)
#define DSI0_CMDQ_SIZE		(DSI0_BASE + 0x060)
#define DSI0_CMDQ		(DSI0_BASE + 0xd00)	/* MT6789-specific: NOT 0x200 */

/* ── Display parameters ─────────────────────────────────────────────── */

#define DISP_WIDTH		1080
#define DISP_HEIGHT		2400
#define DISP_BPP		32
#define DISP_STRIDE		(DISP_WIDTH * (DISP_BPP / 8))
#define DISP_FB_SIZE		(DISP_STRIDE * DISP_HEIGHT)	/* ~10 MB */

/* DSI CMDQ command: DCS 0x2C (Write Memory Start) */
#define DSI_CMDQ_WRITE_MEM_START	0x2c3909

/* OVL_EN bit 22 = shadow reload trigger */
#define OVL_SHADOW_RELOAD		0x00400001

/* Default framebuffer address as set by LK */
#define LK_DEFAULT_FB		0xfeabe000UL

/*
 * Diagnostic RAM buffer — written once at end of probe(), read via md.l
 * after boot through ACM shell.  Placed right after framebuffer end.
 * Format (offset : значение):
 *   +0x00 : magic 0xDEADBEEF
 *   +0x04 : uc_priv->fb  (адрес framebuffer, как его видит vidconsole)
 *   +0x08 : plat->base   (физический адрес, который probe() выставил железу)
 *   +0x0c : uc_priv->fb_size
 *   +0x10 : plat->size
 * Читать: md.l 0xff4a1400 5
 */
#define DIAG_ADDR		(LK_DEFAULT_FB + DISP_FB_SIZE)	/* 0xff4a1400 */

/* ── MMIO accessibility check ────────────────────────────────────────── */

static int mtk_disp_check_mmio(void)
{
	u32 val;

	val = readl(MMSYS_BASE + 0x000);

	if (val == 0xffffffff) {
		printf("mtk_disp: MMSYS MMIO not accessible "
		       "(read 0x%08x at 0x%08lx)\n",
		       val, (ulong)(MMSYS_BASE + 0x000));
		return -ENODEV;
	}

	return 0;
}

/* ── Hardware pipeline init (exact verified sequence) ──────────────── */

static void mtk_disp_pipeline_init(ulong fb_phys)
{
	/*
	 * Шаг 1: включить clock gate биты.
	 * Запись в CLR-регистр (0x108): 1 = снять затвор (включить тактирование).
	 * Используются ТОЛЬКО биты из подтверждённой последовательности.
	 */
	writel(0x420803, MMSYS_CLK_CLR);	/* DSI-related clocks */
	writel(0x4,      MMSYS_CLK_CLR);	/* RDMA0 clock */
	writel(0x80000,  MMSYS_CLK_CLR);	/* MUTEX0 clock */
	writel(0x10000,  MMSYS_CLK_CLR);	/* DITHER0 clock */

	/* Шаг 2: deassert SW reset (write-1-to-deassert) */
	writel(0x1, MMSYS_SW_RST);

	/*
	 * Шаг 3: MMSYS routing — путь через DITHER0 (как в оригинальном LK).
	 * OVL0 → RDMA0 → (COLOR0) → DITHER0 → DSI0
	 */
	writel(0x1, MMSYS_OVL0_MOUT);		/* OVL0 output → RDMA0 */
	writel(0x0, MMSYS_RDMA0_SEL_IN);	/* RDMA0 input ← OVL0 */
	writel(0x1, MMSYS_DITHER0_MOUT);	/* DITHER0 output → DSI0 */
	writel(0x1, MMSYS_DSI0_SEL_IN);	/* DSI0 input ← DITHER0 */

	/* Шаг 4: конфигурация OVL0 Layer 0 */
	writel(0x1,        OVL0_EN);
	writel(0x201e,     OVL0_CFG);
	writel(0x05000001, OVL0_SRC_CON);
	writel(0x1,        OVL0_L0_CON);
	writel(0x1001020,  OVL0_L0_CFG);	/* ARGB8888 */
	writel(0x10e0,     OVL0_L0_PITCH);	/* stride = 1080*4 = 4320 */
	writel(0x09600438, OVL0_L0_SRC_SIZE);	/* 2400<<16 | 1080 */
	writel(0x0,        OVL0_L0_OFFSET);
	writel(fb_phys,    OVL0_L0_ADDR);

	/* Шаг 5: конфигурация RDMA0 */
	writel(0x1f,       RDMA0_EN);
	writel(fb_phys,    RDMA0_MEM_ADDR);
	writel(0x09600438, RDMA0_SRC_SIZE);

	/* Шаг 6: включить MUTEX0 */
	writel(0x1, MUTEX0_EN);

	/* Шаг 7: триггер shadow reload OVL */
	writel(OVL_SHADOW_RELOAD, OVL0_EN);
}

/* ── DSI Write Memory Start trigger ─────────────────────────────────-- */

static void mtk_disp_dsi_trigger(void)
{
	writel(DSI_CMDQ_WRITE_MEM_START, DSI0_CMDQ);
	writel(0x8001, DSI0_CMDQ_SIZE);
	writel(0x0, DSI0_START);
	writel(0x1, DSI0_START);
}

/* ── U-Boot video driver ops ────────────────────────────────────────── */

static int mtk_disp_video_sync(struct udevice *dev)
{
	static int once;
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);

	if (!uc_priv || !uc_priv->fb || !uc_priv->fb_size)
		return 0;

	if (!once) {
		struct video_uc_plat *plat = dev_get_uclass_plat(dev);
		printf("mtk_disp: sync fb=%p plat_base=%lx fb_size=%d "
		       "(match=%s)\n",
		       uc_priv->fb, (ulong)plat->base, uc_priv->fb_size,
		       (ulong)uc_priv->fb == plat->base ? "YES" : "NO");
		once = 1;
	}

	flush_dcache_range((ulong)uc_priv->fb,
			   (ulong)uc_priv->fb + uc_priv->fb_size);
	mtk_disp_dsi_trigger();

	return 0;
}

static int mtk_disp_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	ulong fb_phys;
	int ret;

	printf("mtk_disp: probe\n");

	ret = mtk_disp_check_mmio();
	if (ret) {
		printf("mtk_disp: MMIO not accessible\n");
		return ret;
	}

	fb_phys = readl(OVL0_L0_ADDR);
	if (!fb_phys || fb_phys == 0xffffffff) {
		fb_phys = LK_DEFAULT_FB;
		printf("mtk_disp: using default FB @ 0x%lx\n", fb_phys);
	} else {
		printf("mtk_disp: FB from OVL0_L0_ADDR = 0x%lx\n", fb_phys);
	}

	if (fb_phys < 0x40000000UL || fb_phys > 0x2fffffffffUL) {
		printf("mtk_disp: invalid FB 0x%lx, using 0x%lx\n",
		       fb_phys, (ulong)LK_DEFAULT_FB);
		fb_phys = LK_DEFAULT_FB;
	}

	mtk_disp_pipeline_init(fb_phys);

	plat->base = fb_phys;
	plat->size = DISP_FB_SIZE;

	uc_priv->xsize = DISP_WIDTH;
	uc_priv->ysize = DISP_HEIGHT;
	uc_priv->bpix = VIDEO_BPP32;
	uc_priv->format = VIDEO_ARGB8888;
	uc_priv->line_length = DISP_STRIDE;
	uc_priv->y_start_offset = 200;

	/*
	 * Явно синхронизируем fb/fb_size с plat->base/size.
	 * video_post_probe() сделает то же самое через map_sysmem(),
	 * но uc_priv->fb может быть прочитан vidconsole и до post_probe.
	 */
	uc_priv->fb = (void *)fb_phys;
	uc_priv->fb_size = DISP_FB_SIZE;

	printf("mtk_disp: %dx%d @ 0x%lx\n",
	       DISP_WIDTH, DISP_HEIGHT, fb_phys);
	printf("fb=%p plat_base=%lx\n", uc_priv->fb, (ulong)plat->base);

	/* Диагностика через RAM — без UART */
	writel(0xDEADBEEF, DIAG_ADDR);
	writel((u32)(ulong)uc_priv->fb, DIAG_ADDR + 0x04);
	writel((u32)(ulong)plat->base, DIAG_ADDR + 0x08);
	writel(uc_priv->fb_size, DIAG_ADDR + 0x0c);
	writel(plat->size, DIAG_ADDR + 0x10);
	flush_dcache_range(DIAG_ADDR, DIAG_ADDR + 0x20);

	return 0;
}

static int mtk_disp_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	/*
	 * Do NOT set plat->size here.  Keeping it 0 prevents
	 * video_reserve() from allocating space for a framebuffer that
	 * we will override in probe() with the fixed LK address.
	 */
	plat->align = 0x1000;

	return 0;
}

/* ── Driver definition ──────────────────────────────────────────────── */

static const struct video_ops mtk_disp_ops = {
	.video_sync = mtk_disp_video_sync,
};

static const struct udevice_id mtk_disp_ids[] = {
	{ .compatible = "mediatek,mt6789-disp" },
	{ }
};

U_BOOT_DRIVER(mtk_disp_g99) = {
	.name	= "mtk_disp_g99",
	.id	= UCLASS_VIDEO,
	.of_match = mtk_disp_ids,
	.bind	= mtk_disp_bind,
	.probe	= mtk_disp_probe,
	.ops	= &mtk_disp_ops,
};
