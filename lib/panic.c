/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include <common.h>
#if !defined(CONFIG_PANIC_HANG)
#include <command.h>
#endif
#include "ndb.h"
#include <asm/arch/boot_mode.h>
#include <asm/io.h>
#include <video_rockchip.h>
#include <lcd.h>

#define ALERT_LOOPS 10
int ndb_breakin(int reason, long para1, long para2)
{
	int i;
	static bool ndb_breakin_called = false;
/*	
	if (swd_on_mmc_enabled != true) {
		printf("ntp not inserted\n");
		return -1;
	}
*/
	if(ndb_breakin_called) {
		writel(BOOT_BROM_DOWNLOAD, CONFIG_ROCKCHIP_BOOT_MODE_REG);
#ifndef CONFIG_SPL_BUILD
		for(i = 0; i < ALERT_LOOPS; i++) {		
			rockchip_show_bmp("reburn.bmp");
			udelay(1000*1000);
		}
		ndb_enable_lcd("panic", 0);
		printf("boot panic, entering MaskROM now\n");
#endif		
		do_reset(NULL, 0, 0, NULL);
	}
	ndb_breakin_called = true;

	printf("ntp is inserted, trigger breakin from %p\n", ndb_breakin);
	register long ret asm ("x0");
    // asm("ldr x0, %0"::"r" (para1)); paras are already in registers
    // asm("ldr x1, %0"::"r" (para2));
    switch(reason) {
    case NDB_BRK_UIMM_INIT://         0xE100 
        asm("hlt %0" : : "I" (NDB_BRK_UIMM_INIT));
        break;
    case NDB_BRK_UIMM_OOPS://         0xE107 
        asm("hlt %0" : : "I" (NDB_BRK_UIMM_OOPS));
        break;
    case NDB_BRK_UIMM_MANUAL://         0xE107 
        asm("hlt %0" : : "I" (NDB_BRK_UIMM_MANUAL));
        break;
    case NDB_BRK_UIMM_BOOKED://         0xE107 
        asm("hlt %0" : : "I" (NDB_BRK_UIMM_BOOKED));
        break;
    default:
        asm("hlt %0" : : "I" (NDB_BRK_UIMM_UNKOWN));
    }

    return ret;
};

static void panic_finish(void) __attribute__ ((noreturn));

static void panic_finish(void)
{
	putc('\n');
	ndb_breakin(NDB_BRK_UIMM_OOPS, 1, (long)panic_finish);
#if defined(CONFIG_PANIC_HANG)
	hang();
#else
	udelay(100000);	/* allow messages to go out */
	do_reset(NULL, 0, 0, NULL);
#endif
	while (1)
		;
}

void panic_str(const char *str)
{
	puts(str);
	panic_finish();
}

void panic(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	panic_finish();
}
