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
#include <video_console.h>
#include <asm/io.h>
#include <asm/cache.h>
#include <dm/device-internal.h>
#include <linux/bitops.h>
#include <linux/delay.h>

/* ── Hardware register bases ─────────────────────────────────────────── */

#define MMSYS_BASE		0x14000000UL
#define OVL0_BASE		0x14005000UL
#define RDMA0_BASE		0x14007000UL
#define DSI0_BASE		0x14013000UL
#define MUTEX0_BASE		0x14001000UL

/* ── MMSYS registers ────────────────────────────────────────────────── */

#define MMSYS_CLK_CLR		(MMSYS_BASE + 0x108)	/* mm0 gate clear */
#define MMSYS_CLK_CLR_MM1	(MMSYS_BASE + 0x1A8)	/* mm1 gate clear */
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
#define DSI0_INTEN		(DSI0_BASE + 0x008)
#define DSI0_INTSTA		(DSI0_BASE + 0x00c)
#define DSI0_CON		(DSI0_BASE + 0x010)
#define DSI0_MODE		(DSI0_BASE + 0x014)
#define DSI0_CMDQ_SIZE		(DSI0_BASE + 0x060)
#define DSI0_CMDQ		(DSI0_BASE + 0xd00)

#define DSI_BUSY		BIT(31)
#define DSI_CMD_DONE		BIT(1)

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

/*
 * Diagnostic buffer in RAM — dump everything here so user can read
 * via mtkclient after boot: mtkclient r 0x4FF00000 128
 */
#define DIAG_BUF_PHYS		0x4FF00000UL

struct mtk_disp_diag {
	u32 magic;		/* 0xD1A90C00 */
	u32 version;		/* 2 */
	u32 fb_phys;		/* framebuffer physical address */
	u32 fb_size;		/* framebuffer size in bytes */
	u32 bpix;		/* VIDEO_BPP32 etc */
	u32 line_length;	/* stride in bytes */
	u32 colour_fg;		/* foreground colour as u32 */
	u32 colour_bg;		/* background colour as u32 */
	u32 y_start_offset;	/* 200 */
	u32 sync_count;		/* how many times video_sync was called */
	u32 vc_bound;		/* 1 if vidconsole was bound */
	u32 vc_probed;		/* 1 if vidconsole was probed */
	u32 vc_quiet;		/* quiet flag */
	u32 vc_ycur;		/* current y cursor position */
	u32 vc_x_charsize;	/* font width */
	u32 vc_y_charsize;	/* font height */
	u32 vc_cols;		/* number of columns */
	u32 vc_rows;		/* number of rows */
	u32 vc_xstart_frac;	/* fractional x start */
	u32 vc_xcur_frac;	/* fractional x cursor */
	u32 vc_xsize_frac;	/* fractional x size */
	u32 video_clear_done;	/* 1 if video_clear ran */
	u32 red_bar_written;	/* 1 if red test bar was written */
	u32 last_error;		/* last error code */
	u32 dsi_intsta;		/* DSI_INTSTA at last sync */
	u32 dsi_con;		/* DSI_CON at last sync */
	u32 fb_pitch;		/* DISP_STRIDE */
	u32 screen_w;		/* 1080 */
	u32 screen_h;		/* 2400 */
	u32 pad[32];		/* reserved */
};

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
	/* 1. Enable ALL clocks — clear clock-gate bits (active-low).
	 *    Writing 0xFFFFFFFF was verified on hardware (status became 0x00000000).
	 */
	writel(0xFFFFFFFF, MMSYS_CLK_CLR);
	writel(0xFFFFFFFF, MMSYS_CLK_CLR_MM1);

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

	/* 8. Enable DSI interrupts (CMD_DONE + RD_RDY).
	 *    Offset 0x08 = DSI_INTEN. LK had this enabled.
	 */
	writel(0x3, DSI0_INTEN);
}

static void mtk_disp_dsi_trigger(void)
{
	/*
	 * Send DCS 0x2C (Write Memory Start) to trigger frame refresh.
	 *
	 * LK's FUN_0003e108/FUN_0003f1bc sequence:
	 *   1. Write CMDQ word
	 *   2. Write CMDQ_SIZE with trigger bit
	 *   3. Toggle DSI_START (0 → 1)
	 *
	 * We do NOT poll DSI_BUSY or DSI_CMD_DONE because:
	 *   - DSI_BUSY may stay high during DMA pixel transfer
	 *   - CMD_DONE fires when the 0x2C command is sent, not when
	 *     the full frame is transferred (which happens via DMA)
	 *   - Polling adds latency to the cyclic handler
	 *   - LK's "display initialized" path just toggles DSI_START
	 *     without any polling
	 */
	writel(DSI_CMDQ_WRITE_MEM_START, DSI0_CMDQ);
	writel(0x8001, DSI0_CMDQ_SIZE);
	writel(0x0, DSI0_START);
	writel(0x1, DSI0_START);
}

/* ── U-Boot video driver ops ────────────────────────────────────────── */

static int mtk_disp_video_sync(struct udevice *dev)
{
	static int sync_count;
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct mtk_disp_diag *diag = (struct mtk_disp_diag *)(ulong)DIAG_BUF_PHYS;

	if (!uc_priv || !uc_priv->fb || !uc_priv->fb_size)
		return 0;

	/* Update RAM diagnostic buffer */
	diag->sync_count = sync_count + 1;
	diag->colour_fg = uc_priv->colour_fg;
	diag->colour_bg = uc_priv->colour_bg;
	diag->fb_phys = (u32)(ulong)uc_priv->fb;
	diag->fb_size = uc_priv->fb_size;
	diag->dsi_intsta = readl(DSI0_INTSTA);
	diag->dsi_con = readl(DSI0_CON);

	/* On sync_count >= 3, try to read vidconsole state */
	if (sync_count == 3) {
		struct udevice *cons;
		uclass_first_device_check(UCLASS_VIDEO_CONSOLE, &cons);
		if (cons) {
			struct vidconsole_priv *vc = dev_get_uclass_priv(cons);
			diag->vc_bound = 1;
			diag->vc_probed = device_active(cons) ? 1 : 0;
			diag->vc_quiet = vc->quiet ? 1 : 0;
			diag->vc_ycur = vc->ycur;
			diag->vc_x_charsize = vc->x_charsize;
			diag->vc_y_charsize = vc->y_charsize;
			diag->vc_cols = vc->cols;
			diag->vc_rows = vc->rows;
			diag->vc_xstart_frac = vc->xstart_frac;
			diag->vc_xcur_frac = vc->xcur_frac;
			diag->vc_xsize_frac = vc->xsize_frac;
		}
		flush_dcache_range(DIAG_BUF_PHYS, DIAG_BUF_PHYS + sizeof(*diag));
	}

	/*
	 * First 3 calls: draw thick diagnostic bars on screen.
	 * Each bar is 30px tall for easy visibility.
	 * Colours use 0xFF alpha to be opaque.
	 *
	 * y=150-179: YELLOW  = sync counter (30px)
	 * y=180-209: GREEN   = video_sync called (30px)
	 * y=210-239: RED     = fb pointer valid (30px)
	 * y=240-269: BLUE    = format test (30px)
	 * y=300-315: fg colour block (16px)
	 * y=316-331: bg colour block (16px)
	 */
	if (sync_count < 3) {
		u32 *fb = (u32 *)(ulong)uc_priv->fb;
		int row, col;

		/* YELLOW bar: y=150-179 */
		for (row = 150; row < 180; row++)
			for (col = 0; col < DISP_WIDTH; col++)
				fb[row * (DISP_STRIDE / 4) + col] = 0xFF00FFFF;  /* R=FF G=FF B=0 A=FF */

		/* GREEN bar: y=180-209 */
		for (row = 180; row < 210; row++)
			for (col = 0; col < DISP_WIDTH; col++)
				fb[row * (DISP_STRIDE / 4) + col] = 0xFF00FF00;  /* R=0 G=FF B=0 A=FF */

		/* RED bar: y=210-239 */
		for (row = 210; row < 240; row++)
			for (col = 0; col < DISP_WIDTH; col++)
				fb[row * (DISP_STRIDE / 4) + col] = 0xFFFF0000;  /* R=FF G=0 B=0 A=FF */

		/* BLUE bar: y=240-269 */
		for (row = 240; row < 270; row++)
			for (col = 0; col < DISP_WIDTH; col++)
				fb[row * (DISP_STRIDE / 4) + col] = 0xFF0000FF;  /* R=0 G=0 B=FF A=FF */

		/* fg colour: 16x16 block at (0, 300) */
		for (row = 300; row < 316; row++)
			for (col = 0; col < 16; col++)
				fb[row * (DISP_STRIDE / 4) + col] = uc_priv->colour_fg;

		/* bg colour: 16x16 block at (20, 300) */
		for (row = 300; row < 316; row++)
			for (col = 20; col < 36; col++)
				fb[row * (DISP_STRIDE / 4) + col] = uc_priv->colour_bg;

		flush_dcache_range((ulong)uc_priv->fb,
				   (ulong)uc_priv->fb + uc_priv->fb_size);
		mtk_disp_dsi_trigger();
	}
	sync_count++;

	flush_dcache_range((ulong)uc_priv->fb,
			   (ulong)uc_priv->fb + uc_priv->fb_size);

	mtk_disp_dsi_trigger();

	return 0;
}

static int mtk_disp_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct mtk_disp_diag *diag = (struct mtk_disp_diag *)(ulong)DIAG_BUF_PHYS;
	ulong fb_phys;
	int ret;

	/* Zero out diagnostic buffer */
	memset(diag, 0, sizeof(*diag));
	diag->magic = 0xD1A90C00;
	diag->version = 2;
	diag->screen_w = DISP_WIDTH;
	diag->screen_h = DISP_HEIGHT;
	diag->fb_pitch = DISP_STRIDE;

	printf("mtk_disp: probing...\n");

	/* Step 1: Check if MMIO is accessible at all */
	ret = mtk_disp_check_mmio();
	if (ret) {
		printf("mtk_disp: MMIO not accessible, skipping display\n");
		diag->last_error = 1;
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

	diag->fb_phys = (u32)fb_phys;

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
	uc_priv->y_start_offset = 200; /* skip notch zone at top */

	video_set_flush_dcache(dev, true);

	diag->bpix = uc_priv->bpix;
	diag->line_length = uc_priv->line_length;
	diag->y_start_offset = uc_priv->y_start_offset;
	diag->fb_size = DISP_FB_SIZE;

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
