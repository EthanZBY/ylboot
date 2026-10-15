/*
 * (C) Copyright 2016 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <boot_rkimg.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/boot_mode.h>

DECLARE_GLOBAL_DATA_PTR;

enum {
	PH = 0,	/* P: Priority, H: high, M: middle, L: low*/
	PM,
	PL,
};

static int misc_require_recovery(u32 bcb_offset, int *bcb_recovery_msg)
{
	struct bootloader_message *bmsg;
	struct blk_desc *dev_desc;
	disk_partition_t part;
	int cnt, recovery = 0;

	dev_desc = rockchip_get_bootdev();
	if (!dev_desc) {
		printf("dev_desc is NULL!\n");
		goto out;
	}

	if (part_get_info_by_name(dev_desc, PART_MISC, &part) < 0) {
		printf("No misc partition\n");
		goto out;
	}

	cnt = DIV_ROUND_UP(sizeof(struct bootloader_message), dev_desc->blksz);
	bmsg = memalign(ARCH_DMA_MINALIGN, cnt * dev_desc->blksz);
	if (blk_dread(dev_desc, part.start + bcb_offset, cnt, bmsg) != cnt) {
		recovery = 0;
	} else {
		recovery = !strcmp(bmsg->command, "boot-recovery");
		if (bcb_recovery_msg) {
			if (!strcmp(bmsg->recovery, "recovery\n--rk_fwupdate\n"))
				*bcb_recovery_msg = BCB_MSG_RECOVERY_RK_FWUPDATE;
			else if (!strcmp(bmsg->recovery, "recovery\n--factory_mode=whole") ||
				 !strcmp(bmsg->recovery, "recovery\n--factory_mode=small"))
				*bcb_recovery_msg = BCB_MSG_RECOVERY_PCBA;
		}
	}

	free(bmsg);
out:
	return recovery;
}

int get_bcb_recovery_msg(void)
{
	int bcb_recovery_msg = BCB_MSG_RECOVERY_NONE;
#ifdef CONFIG_ANDROID_BOOT_IMAGE
	u32 bcb_offset = android_bcb_msg_sector_offset();
#else
	u32 bcb_offset = BCB_MESSAGE_BLK_OFFSET;
#endif
	misc_require_recovery(bcb_offset, &bcb_recovery_msg);

	return bcb_recovery_msg;
}

extern bool  krnl_need_breakin;

/*
 * There are three ways to get reboot-mode:
 *
 * No1. Android BCB which is defined in misc.img (0KB or 16KB offset)
 * No2. CONFIG_ROCKCHIP_BOOT_MODE_REG that supports "reboot xxx" commands
 * No3. Env variable "reboot_mode" which is added by U-Boot
 *
 * Recovery mode from:
 *	- Android BCB in misc.img
 *	- "reboot recovery" command
 *	- recovery key pressed without usb attach
 */
int rockchip_get_boot_mode(void)
{
	static int boot_mode[] =		/* static */
		{ -EINVAL, -EINVAL, -EINVAL };
	static int bcb_offset = -EINVAL;	/* static */
	uint32_t reg_boot_mode;
	char *env_reboot_mode;
	int clear_boot_reg = 0;
	int recovery_msg = 0;
#ifdef CONFIG_ANDROID_BOOT_IMAGE
	u32 offset = android_bcb_msg_sector_offset();
#else
	u32 offset = BCB_MESSAGE_BLK_OFFSET;
#endif

	/*
	 * Env variable "reboot_mode" which is added by U-Boot, reading ever time.
	 */
	env_reboot_mode = env_get("reboot_mode");
	if (env_reboot_mode) {
		if (!strcmp(env_reboot_mode, "recovery-key")) {
			printf("boot mode: recovery (key)\n");
			return BOOT_MODE_RECOVERY;
		} else if (!strcmp(env_reboot_mode, "recovery-usb")) {
			printf("boot mode: recovery (usb)\n");
			return BOOT_MODE_RECOVERY;
		} else if (!strcmp(env_reboot_mode, "recovery")) {
			printf("boot mode: recovery (env)\n");
			return BOOT_MODE_RECOVERY;
		} else if (!strcmp(env_reboot_mode, "fastboot")) {
			printf("boot mode: fastboot\n");
			return BOOT_MODE_BOOTLOADER;
		}
	}

	/*
	 * Android BCB special handle:
	 *    Once the Android BCB offset changed, reinitalize "boot_mode[PM]".
	 *
	 * Background:
	 *    1. there are two Android BCB at the 0KB(google) and 16KB(rk)
	 *       offset in misc.img
	 *    2. Android image: return 0KB offset if image version >= 10,
	 *	 otherwise 16KB
	 *    3. Not Android image: return 16KB offset, eg: FIT image.
	 *
	 * To handle the cases of 16KB and 0KB, we reinitial boot_mode[PM] once
	 * Android BCB is changed.
	 *
	 * PH and PL is from boot mode register and reading once.
	 * PM is from misc.img and should be updated if BCB offset is changed.
	 * Return the boot mode according to priority: PH > PM > PL.
	 */
	if (bcb_offset != offset) {
		boot_mode[PM] = -EINVAL;
		bcb_offset = offset;
	}

	/* directly return if there is already valid mode */
	if (boot_mode[PH] != -EINVAL)
		return boot_mode[PH];
	else if (boot_mode[PM] != -EINVAL)
		return boot_mode[PM];
	else if (boot_mode[PL] != -EINVAL)
		return boot_mode[PL];

	/*
	 * Boot mode priority
	 *
	 * Anyway, we should set download boot mode as the highest priority, so:
	 * reboot loader/bootloader/fastboot > misc partition "recovery" > reboot xxx.
	 */
	reg_boot_mode = readl((void *)CONFIG_ROCKCHIP_BOOT_MODE_REG);
	gd->reg_boot = reg_boot_mode - REBOOT_FLAG; // previous write before relocation lost
	if (reg_boot_mode == BOOT_LOADER) {
		printf("boot mode: loader\n");
		boot_mode[PH] = BOOT_MODE_LOADER;
		clear_boot_reg = 1;
	} else if (reg_boot_mode == BOOT_DFU) {
		printf("boot mode: dfu\n");
		boot_mode[PH] = BOOT_MODE_DFU;
		clear_boot_reg = 1;
	} else if (reg_boot_mode == BOOT_FASTBOOT) {
		printf("boot mode: bootloader\n");
		boot_mode[PH] = BOOT_MODE_BOOTLOADER;
		clear_boot_reg = 1;
	} else if (misc_require_recovery(bcb_offset, &recovery_msg)) {
		printf("boot mode: recovery (misc)\n");
		boot_mode[PM] = BOOT_MODE_RECOVERY;
	} else {
		switch (reg_boot_mode) {
		case BOOT_NORMAL:
TAG_BOOT_NORMAL:
			printf("boot mode: normal\n");
			boot_mode[PL] = BOOT_MODE_NORMAL;
			clear_boot_reg = 1;
			break;
		case BOOT_RECOVERY:
			printf("boot mode: recovery (cmd)\n");
			boot_mode[PL] = BOOT_MODE_RECOVERY;
			clear_boot_reg = 1;
			break;
		case BOOT_UMS:
			printf("boot mode: ums\n");
			boot_mode[PL] = BOOT_MODE_UMS;
			clear_boot_reg = 1;
			break;
		case BOOT_CHARGING:
			printf("boot mode: charging\n");
			boot_mode[PL] = BOOT_MODE_CHARGING;
			clear_boot_reg = 1;
			break;
		case BOOT_PANIC:
			printf("boot mode: panic\n");
			boot_mode[PL] = BOOT_MODE_PANIC;
			break;
		case BOOT_WATCHDOG:
			printf("boot mode: watchdog\n");
			boot_mode[PL] = BOOT_MODE_WATCHDOG;
			break;
		case BOOT_NVME:
		case BOOT_NVME_OLD:
			env_set("bootos", "nvme");
			goto TAG_BOOT_NORMAL;
		case BOOT_EMMC:
		case BOOT_EMMC_OLD:
			env_set("bootos", "emmc");
			goto TAG_BOOT_NORMAL;
		case BOOT_SDC:
			env_set("bootos", "sdc");
			goto TAG_BOOT_NORMAL;
		case BOOT_KENRL_5x:
		case BOOT_KRNL5_OLD:
			env_set("bootos", "5-kernel");
			goto TAG_BOOT_NORMAL;
		case BOOT_KENRL_6x:
		case BOOT_KRNL6_OLD:
			env_set("bootos", "6-kernel");
			goto TAG_BOOT_NORMAL;
		case BOOT_KASAN:
		case BOOT_KASAN_OLD:
			env_set("bootos", "kasan");
			goto TAG_BOOT_NORMAL;
		case BOOT_USB:
		case BOOT_USB_OLD:
			env_set("bootos", "usb");
			// writel(BOOT_USB_UPDATE, (void *)CONFIG_ROCKCHIP_BOOT_MODE_REG);
			// run_command("repart mmc", 0); dangerous, disabled on 2025-7-9
			goto TAG_BOOT_NORMAL;
		case BOOT_USB_UPDATE:
			printf("will update emmc by usb\n");
			env_set("bootos", "usb");
			run_command("mw.b 0x10000000 0x12", 0);
			goto TAG_BOOT_NORMAL;
		case BOOT_KDBG:
		case BOOT_KDBG_OLD:
			krnl_need_breakin = true;
		case BOOT_UDBG:
		case BOOT_CONSOLE:
		case BOOT_UDBG_OLD:
		case BOOT_CONSOLE_OLD:
			goto TAG_BOOT_NORMAL;
		default:
			printf("boot mode: None\n");
			boot_mode[PL] = BOOT_MODE_UNDEFINE;
		}
	}

	/*
	 * We don't clear boot mode reg when its value stands for the reboot
	 * reason or others(in the future), the kernel will need and clear it.
	 */
	if (clear_boot_reg)
		writel(BOOT_NORMAL, (void *)CONFIG_ROCKCHIP_BOOT_MODE_REG);

	if (boot_mode[PH] != -EINVAL)
		return boot_mode[PH];
	else if (boot_mode[PM] != -EINVAL)
		return boot_mode[PM];
	else
		return boot_mode[PL];
}

int setup_boot_mode(void)
{
	char env_preboot[256] = {0};

	switch (rockchip_get_boot_mode()) {
	case BOOT_MODE_BOOTLOADER:
		printf("enter fastboot!\n");
#if defined(CONFIG_FASTBOOT_FLASH_MMC_DEV)
		snprintf(env_preboot, 256,
				"setenv preboot; mmc dev %x; fastboot usb 0; ",
				CONFIG_FASTBOOT_FLASH_MMC_DEV);
#elif defined(CONFIG_FASTBOOT_FLASH_NAND_DEV)
		snprintf(env_preboot, 256,
				"setenv preboot; fastboot usb 0; ");
#endif
		env_set("preboot", env_preboot);
		break;
	case BOOT_MODE_UMS:
		printf("enter UMS!\n");
		env_set("preboot", "setenv preboot; ums mmc 0");
		break;
#if defined(CONFIG_CMD_DFU)
	case BOOT_MODE_DFU:
		printf("enter DFU!\n");
		env_set("preboot", "setenv preboot; dfu 0 ${devtype} ${devnum}; rbrom");
		break;
#endif
	case BOOT_MODE_LOADER:
		printf("enter Rockusb!\n");
		env_set("preboot", "setenv preboot; rockusb 0 ${devtype} ${devnum}; rbrom");
		break;
	case BOOT_MODE_CHARGING:
		printf("enter charging!\n");
		env_set("preboot", "setenv preboot; charge");
		break;
	}

	return 0;
}
