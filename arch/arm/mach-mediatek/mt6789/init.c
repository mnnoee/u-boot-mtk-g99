// SPDX-License-Identifier: GPL-2.0

#include <asm/armv8/mmu.h>
#include <asm/global_data.h>
#include <asm/system.h>
#include <dm/uclass.h>
#include <fdtdec.h>
#include <early_log.h>
#include <init.h>
#include <linux/sizes.h>
#include <wdt.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	int ret;
	EARLY_W("mt6789:dram_init:entry");
	ret = fdtdec_setup_mem_size_base();
	EARLY_W("mt6789:dram_init:exit");
	return ret;
}

int dram_init_banksize(void)
{
	// This is a HACK!
	// We need to write a logic parsing the actual amount of banks from DT
	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = SZ_4G;

	gd->bd->bi_dram[1].start = SZ_4G;
	gd->bd->bi_dram[1].size = gd->ram_size;

	return 0;
}

int mtk_soc_early_init(void)
{
	EARLY_W("mt6789:mtk_soc_early_init");
	return 0;
}

void reset_cpu(void)
{
	struct udevice *wdt;

	if (IS_ENABLED(CONFIG_PSCI_RESET)) {
		psci_system_reset();
	} else {
		uclass_first_device(UCLASS_WDT, &wdt);
		if (wdt)
			wdt_expire_now(wdt, 0);
	}
}

int print_cpuinfo(void)
{
	printk("CPU:   MediaTek Helio G99 (MT6789)\n");
	return 0;
}

static struct mm_region mt6789_mem_map[] = {
	{
		/* Peripheral region (DRAM bank 0) */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM bank 1 */
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* EMI Reserved  */
		.virt = 0x7d000000UL,
		.phys = 0x7d000000UL,
		.size = 0x00500000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* SSPM-AP Reserved */
		.virt = 0x9f560000UL,
		.phys = 0x9f560000UL,
		.size = 0x00110000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* CCCI Reserved  */
		.virt = 0xbf9f0000UL,
		.phys = 0xbf9f0000UL,
		.size = 0x00010000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* SCP Reserved  */
		.virt = 0xbfa00000UL,
		.phys = 0xbfa00000UL,
		.size = 0x00400000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* CRYPTO Reserved  */
		.virt = 0x48401000UL,
		.phys = 0x48401000UL,
		.size = 0x00001000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* TEE Reserved 0 */
		.virt = 0x70000000UL,
		.phys = 0x70000000UL,
		.size = 0x04600000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* TEE Reserved 1 */
		.virt = 0x7de00000UL,
		.phys = 0x7de00000UL,
		.size = 0x00800000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* MCUPM Reserved */
		.virt = 0x7feb0000UL,
		.phys = 0x7feb0000UL,
		.size = 0x00100000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* SSPM Reserved */
		.virt = 0x9fe70000UL,
		.phys = 0x9fe70000UL,
		.size = 0x00180000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* BL31 Reserved */
		.virt = 0x48200000UL,
		.phys = 0x48200000UL,
		.size = 0x00200000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* Framebuffer Reserved */
		.virt = 0xfe0c8000UL,
		.phys = 0xfe0c8000UL,
		.size = 0x01f2f000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
		PTE_BLOCK_INNER_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
	{
		/* AEE Reserved */
		.virt = 0x50700000UL,
		.phys = 0x50700000UL,
		.size = 0x00800000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* GZ Reserved */
		.virt = 0x7ec00000UL,
		.phys = 0x7ec00000UL,
		.size = 0x01200000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 3 */
		.virt = 0xfecd0000UL,
		.phys = 0xfecd0000UL,
		.size = 0x4132f000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* DRAM Controller Reserved */
		.virt = 0x03ffff000UL,
		.phys = 0x03ffff000UL,
		.size = 0x000001000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		0,
	}
};

struct mm_region *mem_map = mt6789_mem_map;
