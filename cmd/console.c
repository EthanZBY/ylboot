/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <stdio_dev.h>

DECLARE_GLOBAL_DATA_PTR;	// added by RaymondZ on 2025-5-29

extern void _do_coninfo (void);
static int do_coninfo(cmd_tbl_t *cmd, int flag, int argc, char * const argv[])
{
	int l;
	struct list_head *list = stdio_get_list();
	struct list_head *pos;
	struct stdio_dev *dev;

	/* Scan for valid output and input devices */

	puts ("List of available devices:\n");

	list_for_each(pos, list) {
		dev = list_entry(pos, struct stdio_dev, list);

		printf ("%-8s %08x %c%c ",
			dev->name,
			dev->flags,
			(dev->flags & DEV_FLAGS_INPUT) ? 'I' : '.',
			(dev->flags & DEV_FLAGS_OUTPUT) ? 'O' : '.');

		for (l = 0; l < MAX_FILES; l++) {
			if (stdio_devices[l] == dev) {
				printf ("%s ", stdio_names[l]);
			}
		}
		putc ('\n');
	}
	
	stdio_print_current_devices(); // added by RaymondZ on 2025-5-29

	printf("gd->flags	= %08x\n", gd->flags);
	printf("gd->flags & GD_FLG_SERIAL_READY	= %08x\n", gd->flags & GD_FLG_SERIAL_READY);
	if(argc > 1) {
		gd->flags ^= GD_FLG_SERIAL_READY;
		printf("GD_FLG_SERIAL_READY	flags is toggled to = %08x\n", gd->flags & GD_FLG_SERIAL_READY);
	}
	return 0;
}

/***************************************************/

U_BOOT_CMD(
	coninfo,	3,	1,	do_coninfo,
	"print console devices and information",
	""
);
