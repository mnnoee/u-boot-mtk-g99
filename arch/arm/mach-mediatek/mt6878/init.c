// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <fdtdec.h>
#include <init.h>
#include <asm/armv8/mmu.h>
#include <asm/system.h>
#include <asm/global_data.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

int print_cpuinfo(void)
{
	return 0;
//	printk("CPU:   MediaTek MT6878\n");
//	return 0;
}

int dram_init(void)
{
	int ret;

	ret = fdtdec_setup_mem_size_base();
	if (ret)
		return ret;

	// Fix ram_size to 2GB, above that things start to break
	gd->ram_size = get_ram_size((void *)gd->ram_base, SZ_2G);

	return 0;
}

int dram_init_banksize(void)
{
	/*
	 * X676B: 8 GiB of DRAM, contiguous from 0x40000000.
	 * gd->ram_size is deliberately capped at 2 GiB above: that is what
	 * U-Boot itself uses (heap/stack/relocation).  The kernel needs the
	 * real layout, otherwise it sees only 2 GiB.
	 *   bank0: 0x40000000..0x100000000  (3 GiB)
	 *   bank1: 0x100000000..0x240000000 (5 GiB)
	 * Reserved regions (TEE, modem, wifi, sspm) are described in the
	 * Linux DTB reserved-memory and subtracted by the kernel.
	 */
	gd->bd->bi_dram[0].start = 0x40000000ULL;
	gd->bd->bi_dram[0].size = 0xC0000000ULL;   /* 3 GiB, up to 4 GiB */
	gd->bd->bi_dram[1].start = 0x100000000ULL;
	gd->bd->bi_dram[1].size = 0x140000000ULL;  /* 5 GiB, 8 GiB total */

	return 0;
}

void reset_cpu(void)
{
	psci_system_reset();
}

static struct mm_region tetris_mem_map[] = {
	{
		/* Peripheral region */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x40000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 0 */
		.virt = 0x040000000UL,
		.phys = 0x040000000UL,
		.size = 0x026f00000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* TEE Reserved 0 */
		.virt = 0x66f00000UL,
		.phys = 0x66f00000UL,
		.size = 0x09100000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 1 */
		.virt = 0x70000000UL,
		.phys = 0x70000000UL,
		.size = 0x08000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* TEE Reserved 1 */
		.virt = 0x78000000UL,
		.phys = 0x78000000UL,
		.size = 0x07fff000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 2 */
		.virt = 0x7ffff000UL,
		.phys = 0x7ffff000UL,
		.size = 0x7e0d1000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* Framebuffer Reserved */
		.virt = 0xfe0d0000UL,
		.phys = 0xfe0d0000UL,
		.size = 0x00c00000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
				 PTE_BLOCK_INNER_SHARE |
				 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	},
	{
		/* DRAM region 3 */
		.virt = 0xfecd0000UL,
		.phys = 0xfecd0000UL,
		.size = 0x4132f000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		/* DRAM Controller 0 Reserved */
		.virt = 0x13ffff000UL,
		.phys = 0x13ffff000UL,
		.size = 0x000001000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE |
		PTE_BLOCK_PXN | PTE_BLOCK_UXN,
	},
	{
		/* DRAM region 4 */
		.virt = 0x140000000UL,
		.phys = 0x140000000UL,
		.size = 0x0c0000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
},
	{
		/* X676B: top 1 GiB (mtest check, 0x200000000..0x240000000) */
		.virt = 0x200000000UL,
		.phys = 0x200000000UL,
		.size = 0x040000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE
	},
	{
		0,
	}
};

struct mm_region *mem_map = tetris_mem_map;
