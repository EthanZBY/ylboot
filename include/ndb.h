/*
Header for NanoDebugger (NDB) by RaymondZhang in GSL 2024-11-22
*/
#ifndef __NDB_H__
#define __NDB_H__
#include <stdbool.h>

// 0xE000 is used by kernel
#define NDB_BRK_UIMM_INIT         0xB000 
#define NDB_BRK_UIMM_OOPS         0xA000
#define NDB_BRK_UIMM_UNKOWN       0x9000
#define NDB_BRK_UIMM_MANUAL       0x8000
#define NDB_BRK_UIMM_BOOKED       0x7000

int ndb_debug_init(void);
int ndb_breakin(int reason, long para1, long para2);
void ndb_enable_lcd(const char* msg, int errcode);
extern bool swd_on_mmc_enabled;

#define RK3588_SDMMC_BASE					0xfe2c0000
#define RK3588_BUS_IOC_BASE					0xfd5f8000
#define RK3588_SDMMC_CDETECT				0x0050
#define RK3588_BUS_IOC_GPIO4D_IOMUX_SEL_L	0x0098
#define RK_GPIO0_A4							4

#define RK3588_IRAM_BASE					0xff090000
#define YL_NDB_NOTE_OFFSET					0x10
#define NDB_NOTE_KERNEL_BASE				0x88880000

#define GRF_HIWORD_UPDATE(val, mask, shift) \
			((val) << (shift) | (mask) << ((shift) + 16))

#define YL_GENUINE() ((gd->ddr_in_gb) >= 15)

#endif // __NDB_H__