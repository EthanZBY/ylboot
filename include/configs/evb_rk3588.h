/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd
 */

#ifndef __CONFIGS_RK3588_EVB_H
#define __CONFIGS_RK3588_EVB_H

#include <configs/rk3588_common.h>

#ifndef CONFIG_SPL_BUILD

/* Keep early malloc/GD/stack clear of the U-Boot payload and appended DTBs.
 * rk3588-yourland reserves 512 KiB for early malloc below this stack top.
 */
#undef CONFIG_SYS_INIT_SP_ADDR
#define CONFIG_SYS_INIT_SP_ADDR		0x00800000

#undef ROCKCHIP_DEVICE_SETTINGS
#define ROCKCHIP_DEVICE_SETTINGS \
		"stdin=serial\0" \
		"stdout=serial,vidconsole\0" \
		"stderr=serial,vidconsole\0"

#define CONFIG_SYS_MMC_ENV_DEV		0

#define CONFIG_MMCROOT                  "/dev/mmcblk0p3"

#define CONFIG_PHY_ADDR           1
#define CONFIG_SYS_RTC_BUS_NUM          0

#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
		"bootos=nvme\0" \
		"ini_not_exits=1\0" \
        "fdt_addr_r=0x17000000\0" \
        "loadaddr_=0x12000000\0" \
        "loadaddr=0x14000000\0" \
        "initrd_addr=0x18000000\0" \
        "bootdir=/boot/\0" \
        "image=Image.gz\0" \
        "console=ttyFIQ0\0" \
        "fdt_high=0xffffffff\0" \
        "initrd_high=0xffffffff\0" \
        "fdt_file=x5.dtb\0" \
        "rd_file=ramdisk.img\0" \
        "quiet=quiet\0" \
        "ip_dyn=yes\0" \
        "mmcdev=0\0" \
        "mmcpart=2\0" \
        "mmcroot=" CONFIG_MMCROOT " \0" \
        "mmcautodetect=yes\0" \
        "mmcargs=setenv bootargs console=${console},${baudrate} " \
                "root=${mmcroot} rootfstype=ext4 rootwait rw quiet splash rootwait\0" \
        "loadimage=load mmc ${mmcdev}:${mmcpart} ${loadaddr_} ${bootdir}${image};unzip ${loadaddr_} ${loadaddr}\0" \
        "loadfdt=load mmc ${mmcdev}:${mmcpart} ${fdt_addr_r} ${bootdir}${fdt_file};fdt addr ${fdt_addr_r}\0" \
        "loadrd=load mmc ${mmcdev}:${mmcpart} ${initrd_addr} ${bootdir}${rd_file}\0" \
        "mmcboot=echo Booting from mmc ...; " \
                "run mmcargs; " \
                "if run loadfdt; then " \
                        "booti ${loadaddr} - ${fdt_addr_r}; " \
                "else " \
                        "echo WARN: Cannot load the DT; " \
                         "echo; " \
                "fi;\0" \
		"nvmeroot=/dev/nvme0n1p2\0" \
		"nvmeargs=setenv bootargs root=${nvmeroot} rootfstype=ext4 rw console=ttyFIQ0,1500000 psi=1 rootwait quiet splash earlyprintk earlyprintk=ttyFIQ0,1500000 verbose nokaslr \0" \
		"cnsoargs=setenv bootargs root=${nvmeroot} console=${console},${baudrate} psi=1 splash verbose nokaslr \0" \
		"bootargs_add=\0" \
		"sdcroot=/dev/mmcblk1p2\0" \
		"sdcargs=setenv bootargs root=${sdcroot} rootfstype=ext4 rw console=${console},${baudrate} quiet rootwait psi=1 splash verbose nokaslr \0" \
		"usbargs=setenv bootargs ${smp} " \
                "root=${ramroot} " \
                "rootfstype=${ramrootfstype} " \
                "net.ifnames=0 biosdevname=0 label=install \0" \
        "ramroot=/dev/ram0 rw\0" \
        "ramrootfstype=ext4\0" \
        "ramargs=setenv bootargs ${smp} " \
                "root=${ramroot} " \
                "rootfstype=${ramrootfstype} " \
                "net.ifnames=0 biosdevname=0 splash ${quiet} plymouth.ignore-serial-consoles \0" \
        "ramboot=run ramargs; " \
                "mmc dev ${mmcdev}; " \
                "if run loadimage; then " \
                        "if run loadfdt; then " \
                                "if run loadrd; then " \
                                        "booti ${loadaddr} ${initrd_addr} ${fdt_addr_r}; " \
                                "else echo; "\
                                "fi; " \
                        "else echo; " \
                        "fi; " \
                "else echo; "\
                "fi;\0" \
	ENV_MEM_LAYOUT_SETTINGS \
	ROCKCHIP_DEVICE_SETTINGS 

#if 0
#ifdef CONFIG_USB_KEYBOARD
#undef CONFIG_PREBOOT
/*#define CONFIG_PREBOOT "usb start;setenv stdin usbkbd,serial;"*/
#define CONFIG_PREBOOT "gpio set 113;gpio set 112;"
#endif /* CONFIG_USB_KEYBOARD */
#endif

#undef CONFIG_BOOTCOMMAND
#ifdef GDYL_SKYSI_BOOT
        #define CONFIG_BOOTCOMMAND \
                "repart mmc;bootyl;reset;" 
#else
        #define CONFIG_BOOTCOMMAND \
                "bootyl;reset;" 
#endif
#endif
#endif
