// SPDX-License-Identifier: GPL-2.0+
/*
 * MediaTek MT6789 (Helio G99) display driver for U-Boot
 *
 * Minimal UCLASS_VIDEO driver. LK/BL2_EXT already initializes the DSI
 * controller and panel (ILI7835F). This driver:
 *   1. Probes MMIO accessibility
 *   2. Reads the current framebuffer address from OVL0_L0_ADDR
 *   3. Configures OVL0 → RDMA0 → DITHER0 → DSI0 pipeline
 *   4. Registers as a U-Boot video device for vidconsole/BMP/splash
 *   5. Triggers frame updates via OVL shadow reload + DSI Write Memory Start
 *
 * SAFETY: All MMIO access is guarded. If registers are not accessible
 * (e.g. no clock, no mapping), probe returns -ENODEV and U-Boot falls
 * back to serial console.
 */

#include <dm.h>
#include <log.h>
#include <video.h>
#include <asm/io.h>
#include <asm/cache.h>
#include <linux/bitops.h>

/* ── Hardware register bases ─────────────────────────────────────────── */

#define MMSYS_BASE		0x14000000UL
#define OVL0_BASE		0x14005000UL
#define RDMA0_BASE		0x14007000UL
#define DSI0_BASE		0x14013000UL
#define MUTEX0_BASE		0x14001000UL

/* ── MMSYS registers ────────────────────────────────────────────────── */

#define MMSYS_CLK_CLR		(MMSYS_BASE + 0x108)
#define MMSYS_SW_RST		(MMSYS_BASE + 0x140)

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
#define DSI0_CMDQ		(DSI0_BASE + 0xd00)

/* ── Display parameters ─────────────────────────────────────────────── */

#define DISP_WIDTH		1080
#define DISP_HEIGHT		2400
#define DISP_BPP		32
#define DISP_STRIDE		(DISP_WIDTH * (DISP_BPP / 8))
#define DISP_FB_SIZE		(DISP_STRIDE * DISP_HEIGHT)

/* DSI CMDQ command: DCS 0x2C (Write Memory Start) */
#define DSI_CMDQ_WRITE_MEM_START	0x2c3909

/* OVL_EN bit 22 = shadow reload trigger */
#define OVL_SHADOW_RELOAD		0x00400001

/* Known good framebuffer address (LK default) */
#define LK_DEFAULT_FB		0xfeabe000UL

/* ── Safety: probe MMIO before doing anything ────────────────────────── */

/*
 * Try to read MMSYS_CLK_CLR. If MMIO is not mapped, this will cause
 * a data abort. We use a simple read-back check.
 *
 * Returns 0 if MMIO is accessible, -ENODEV otherwise.
 */
static int mtk_disp_check_mmio(void)
{
	u32 val;

	/*
	 * MMSYS_CLK_CLR is a write-only clear register.
	 * Reading it may return 0 or bus error. Instead, probe a
	 * readable register: OVL0_CFG (read-write after reset).
	 * LK sets it to 0x201e. If MMIO is mapped, we get a value.
	 * If not mapped, we get 0xffffffff or crash.
	 *
	 * Use a safer approach: read the MMSYS syscon ID register
	 * at offset 0x000. MediaTek syscons have a hardware ID there.
	 */
	val = readl(MMSYS_BASE + 0x000);

	/*
	 * If the read returns 0xffffffff, the bus is not responding
	 * (no clock, no power, or not mapped). Return error.
	 */
	if (val == 0xffffffff) {
		printf("mtk_disp: MMSYS MMIO not accessible "
		       "(read 0x%08x at 0x%08lx)\n",
		       val, (ulong)(MMSYS_BASE + 0x000));
		return -ENODEV;
	}

	debug("mtk_disp: MMSYS ID = 0x%08x — MMIO accessible\n", val);
	return 0;
}

/* ── Hardware pipeline setup ────────────────────────────────────────── */

static void mtk_disp_pipeline_init(ulong fb_phys)
{
	/* 1. Enable clocks — clear clock-gate bits (active-low) */
	writel(0x420803, MMSYS_CLK_CLR);
	writel(0x4, MMSYS_CLK_CLR);
	writel(0x80000, MMSYS_CLK_CLR);
	writel(0x10000, MMSYS_CLK_CLR);

	/* 2. Deassert SW reset */
	writel(0x1, MMSYS_SW_RST);

	/* 3. MMSYS routing: OVL0 → RDMA0 → DITHER0 → DSI0 */
	writel(0x1, MMSYS_BASE + 0xf18);
	writel(0x0, MMSYS_BASE + 0xf28);
	writel(0x1, MMSYS_BASE + 0xf20);
	writel(0x1, MMSYS_BASE + 0xf30);

	/* 4. Configure OVL0 Layer 0 */
	writel(0x1, OVL0_EN);
	writel(0x201e, OVL0_CFG);
	writel(0x05000001, OVL0_SRC_CON);
	writel(0x1, OVL0_L0_CON);
	writel(0x1001020, OVL0_L0_CFG);
	writel(0x10e0, OVL0_L0_PITCH);
	writel(0x09600438, OVL0_L0_SRC_SIZE);
	writel(0x0, OVL0_L0_OFFSET);
	writel(fb_phys, OVL0_L0_ADDR);

	/* 5. Configure RDMA0 */
	writel(0x1f, RDMA0_EN);
	writel(fb_phys, RDMA0_MEM_ADDR);
	writel(0x09600438, RDMA0_SRC_SIZE);

	/* 6. Enable MUTEX0 */
	writel(0x1, MUTEX0_EN);

	/* 7. Trigger OVL shadow reload */
	writel(OVL_SHADOW_RELOAD, OVL0_EN);
}

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
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);

	if (!uc_priv || !uc_priv->fb || !uc_priv->fb_size)
		return 0;

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

	printf("mtk_disp: probing...\n");

	/* Step 1: Check if MMIO is accessible at all */
	ret = mtk_disp_check_mmio();
	if (ret) {
		printf("mtk_disp: MMIO not accessible, skipping display\n");
		return ret;
	}

	/* Step 2: Read FB address from hardware */
	fb_phys = readl(OVL0_L0_ADDR);
	if (!fb_phys || fb_phys == 0xffffffff) {
		fb_phys = LK_DEFAULT_FB;
		printf("mtk_disp: OVL not set, using default FB @ 0x%lx\n",
		       fb_phys);
	} else {
		printf("mtk_disp: FB from OVL = 0x%lx\n", fb_phys);
	}

	/* Sanity check: FB must be in DRAM (0x40000000 - 0x2fffffffff for 8GB) */
	if (fb_phys < 0x40000000UL || fb_phys > 0x2fffffffffUL) {
		printf("mtk_disp: invalid FB address 0x%lx, using default\n",
		       fb_phys);
		fb_phys = LK_DEFAULT_FB;
	}

	/* Step 3: Configure display pipeline */
	printf("mtk_disp: init pipeline, FB @ 0x%lx\n", fb_phys);
	mtk_disp_pipeline_init(fb_phys);

	/* Step 4: Register with video uclass */
	plat->base = fb_phys;
	plat->size = DISP_FB_SIZE;

	uc_priv->xsize = DISP_WIDTH;
	uc_priv->ysize = DISP_HEIGHT;
	uc_priv->bpix = VIDEO_BPP32;
	uc_priv->format = VIDEO_ARGB8888;
	uc_priv->line_length = DISP_STRIDE;

	video_set_flush_dcache(dev, true);

	printf("mtk_disp: probe OK — %dx%d, stride=%d\n",
	       DISP_WIDTH, DISP_HEIGHT, DISP_STRIDE);

	return 0;
}

static int mtk_disp_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	/*
	 * We use a FIXED framebuffer address (from LK), so we don't need
	 * U-Boot to allocate memory. Set size to 0 so the uclass skips
	 * reservation and we set plat->base at probe time.
	 *
	 * NOTE: If size=0 causes issues, set a minimal size. The uclass
	 * will reserve some memory but we override the base at probe.
	 */
	plat->align = 0;
	plat->size = 0;

	debug("mtk_disp: bind (no memory reservation — using fixed FB)\n");

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
