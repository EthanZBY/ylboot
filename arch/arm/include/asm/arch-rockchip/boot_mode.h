#ifndef __REBOOT_MODE_H
#define __REBOOT_MODE_H

/* high 24 bits is tag, low 8 bits is type */
#define REBOOT_FLAG		0x5242C300
/* cold boot */
#define BOOT_COLD		0
/* normal boot */
#define BOOT_NORMAL		(REBOOT_FLAG + 0)
/* enter loader rockusb mode */
#define BOOT_LOADER		(REBOOT_FLAG + 1)
/* enter recovery */
#define BOOT_RECOVERY		(REBOOT_FLAG + 3)
/* reboot by panic */
#define BOOT_PANIC		(REBOOT_FLAG + 7)
/* reboot by watchdog */
#define BOOT_WATCHDOG		(REBOOT_FLAG + 8)
/* enter fastboot mode */
#define BOOT_FASTBOOT		(REBOOT_FLAG + 9)
/* enter charging mode */
#define BOOT_CHARGING		(REBOOT_FLAG + 11)
/* enter usb mass storage mode */
#define BOOT_UMS		(REBOOT_FLAG + 12)
/* enter dfu download mode */
#define BOOT_DFU                (REBOOT_FLAG + 13)
/* reboot system quiescent */
#define BOOT_QUIESCENT		(REBOOT_FLAG + 14)
/* reboot by panic and capture ramdump in uboot through usb */
#define BOOT_WINUSB		(REBOOT_FLAG + 15)

#define BOOT_NVME_OLD                (REBOOT_FLAG + 16)
#define BOOT_EMMC_OLD                (REBOOT_FLAG + 17)
#define BOOT_SDC_OLD                 (REBOOT_FLAG + 18)
#define BOOT_USB_OLD                 (REBOOT_FLAG + 19)
#define BOOT_CONSOLE_OLD             (REBOOT_FLAG + 20)
#define BOOT_KDBG_OLD                 (REBOOT_FLAG + 21)
#define BOOT_KRNL5_OLD                 (REBOOT_FLAG + 22)
#define BOOT_KRNL6_OLD                 (REBOOT_FLAG + 23)
#define BOOT_KASAN_OLD                 (REBOOT_FLAG + 24)
#define BOOT_UDBG_OLD                 (REBOOT_FLAG + 25)
#define BOOT_VDBG_OLD                 (REBOOT_FLAG + 27)

// extended by GEDU, starting from 0x60
#define BOOT_MODE_GEDU_BASE      (REBOOT_FLAG + 0x60)

#define BOOT_NVME                (BOOT_MODE_GEDU_BASE + 0)
#define BOOT_EMMC                (BOOT_MODE_GEDU_BASE + 1)
#define BOOT_SDC                 (BOOT_MODE_GEDU_BASE + 2)
#define BOOT_USB                 (BOOT_MODE_GEDU_BASE + 3)
#define BOOT_CONSOLE             (BOOT_MODE_GEDU_BASE + 4)
#define BOOT_KDBG                (BOOT_MODE_GEDU_BASE + 5)
#define BOOT_KENRL_5x            (BOOT_MODE_GEDU_BASE + 6)
#define BOOT_KENRL_6x            (BOOT_MODE_GEDU_BASE + 7)
#define BOOT_KASAN               (BOOT_MODE_GEDU_BASE + 8)
#define BOOT_UDBG                (BOOT_MODE_GEDU_BASE + 9)
#define BOOT_ADBG                (BOOT_MODE_GEDU_BASE + 10) // ATF
#define BOOT_VDBG                (BOOT_MODE_GEDU_BASE + 11) // VMM

/* enter bootrom download mode */
#define BOOT_BROM_DOWNLOAD	0xEF08A53C
// update from usb disk, trigger by 5 presses of power button
#define BOOT_USB_UPDATE     0xEF08A53D

#ifndef __ASSEMBLY__
int setup_boot_mode(void);
#endif

enum {
	BCB_MSG_RECOVERY_NONE,
	BCB_MSG_RECOVERY_RK_FWUPDATE,
	BCB_MSG_RECOVERY_PCBA,
};

#endif
