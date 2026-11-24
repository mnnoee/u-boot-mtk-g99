// SPDX-License-Identifier: GPL-2.0+
/*
 * usbcon - attach/detach the USB CDC ACM console gadget.
 *
 * The download gadget (g_dnl) can only serve one function at a time, so the
 * ACM console has to be released before fastboot or UMS can take over the
 * UDC, and re-registered afterwards.
 */
#include <command.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <stdio_dev.h>

static int do_usbcon(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "off")) {
		ret = drv_usbacm_exit();
		if (ret && ret != -ENODEV) {
			printf("usbcon: cannot release ACM console (%d)\n", ret);
			return CMD_RET_FAILURE;
		}
		printf("usbcon: ACM console released\n");
		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "on")) {
		ret = drv_usbacm_init();
		if (ret) {
			printf("usbcon: cannot register ACM console (%d)\n", ret);
			return CMD_RET_FAILURE;
		}
		printf("usbcon: ACM console registered\n");
		return CMD_RET_SUCCESS;
	}

	return CMD_RET_USAGE;
}

U_BOOT_CMD(usbcon, 2, 0, do_usbcon,
	   "USB CDC ACM console gadget",
	   "on|off - register or release the ACM console gadget");
