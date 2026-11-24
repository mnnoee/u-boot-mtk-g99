// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Igor Belwon <igor.belwon@mentallysanemainliners.org>
 */

#include <config.h>
#include <command.h>
#include <env.h>
#include <init.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <asm/system.h>
#include <fastboot.h>
#include <string.h>

DECLARE_GLOBAL_DATA_PTR;

void __weak fastboot_oem_board(char *cmd_parameter, void *data, u32 size, char *response)
{
	if(strncmp(cmd_parameter, "poweroff", 10) == 0)
	{
		fastboot_okay("goodbye :(", response);
		psci_system_off();
	}
	else
	{
		fastboot_fail("unknown oem_board command", response);
	}
}

/*
 * PMIC access through the WACS2 bridge.
 *
 * The preloader initialises pwrap and leaves WACS2 ready, so a full pwrap
 * driver is not needed: LK uses the same registers for its battery and
 * regulator accesses.  Register addresses are the MT6358/MT6366 LDO
 * definitions from the Linux driver (include/linux/mfd/mt6358/registers.h).
 */
#define WACS2_CMD		0x10026880
#define WACS2_WDATA		0x10026884
#define WACS2_RDATA		0x10026894
#define WACS2_VLDCLR		0x100268a4
#define WACS2_FSM		0x100268a8

#define PMIC_VMC_CON0		0x1cc4
#define PMIC_VMC_ANA_CON0	0x1e4c
#define PMIC_VMCH_CON0		0x1cd8
#define PMIC_VMCH_ANA_CON0	0x1e48

#define WACS2_TIMEOUT		100000

static int wacs2_wait(u32 mask, u32 val)
{
	u32 t = WACS2_TIMEOUT;

	while (t--) {
		if ((readl((void *)WACS2_FSM) & mask) == val)
			return 0;
	}
	return -ETIMEDOUT;
}

static int pmic_read(u16 addr, u16 *val)
{
	if (wacs2_wait(0xe, 0))
		return -ETIMEDOUT;

	writel(addr, (void *)WACS2_CMD);
	if (wacs2_wait(0xe, 0xc))
		return -ETIMEDOUT;

	*val = readl((void *)WACS2_RDATA) & 0xffff;
	writel(1, (void *)WACS2_VLDCLR);
	return 0;
}

static int pmic_write(u16 addr, u16 val)
{
	if (wacs2_wait(0xe, 0))
		return -ETIMEDOUT;

	writel(val, (void *)WACS2_WDATA);
	writel(addr | BIT(29), (void *)WACS2_CMD);
	return 0;
}

static void pmic_update_bits(u16 addr, u16 mask, u16 val)
{
	u16 old;

	if (pmic_read(addr, &old))
		return;
	pmic_write(addr, (old & ~mask) | (val & mask));
}

/*
 * Power the SD slot: VMCH is the card supply (2.9 V), VMC the I/O supply
 * (1.8 V), matching the values Android uses on this board.
 */
static void x676b_sd_power_on(void)
{
	/* VMCH: vosel[10:8] = 2 -> 2.9 V */
	pmic_update_bits(PMIC_VMCH_ANA_CON0, 0x700, 2 << 8);
	pmic_update_bits(PMIC_VMCH_CON0, BIT(0), BIT(0));

	/* VMC: vosel[11:8] = 4 -> 1.8 V */
	pmic_update_bits(PMIC_VMC_ANA_CON0, 0xf00, 4 << 8);
	pmic_update_bits(PMIC_VMC_CON0, BIT(0), BIT(0));
}

static int do_sdpower(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	u16 v;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "read")) {
		pmic_read(PMIC_VMCH_CON0, &v);
		printf("vmch con0 = 0x%04x\n", v);
		pmic_read(PMIC_VMCH_ANA_CON0, &v);
		printf("vmch ana  = 0x%04x\n", v);
		pmic_read(PMIC_VMC_CON0, &v);
		printf("vmc  con0 = 0x%04x\n", v);
		pmic_read(PMIC_VMC_ANA_CON0, &v);
		printf("vmc  ana  = 0x%04x\n", v);
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "on")) {
		x676b_sd_power_on();
		printf("sdpower: slot powered\n");
		return CMD_RET_SUCCESS;
	}

	return CMD_RET_USAGE;
}

U_BOOT_CMD(sdpower, 2, 1, do_sdpower,
	   "SD slot PMIC power (MT6358/MT6366 via WACS2)",
	   "read|on");

int board_late_init(void)
{
	x676b_sd_power_on();
	return 0;
}
