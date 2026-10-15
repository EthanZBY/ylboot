#include <common.h>
#include <malloc.h>
#include <environment.h>
#include <linux/ctype.h>
#include <asm/byteorder.h>
#include <fastboot.h>
#include <fb_mmc.h>
#include <stdlib.h>
#include <version.h>
#include <uboot_aes.h>
#include <boot_rkimg.h>
#include <asm/io.h>
#ifdef CONFIG_SANDBOX
	#define BOOT_BROM_DOWNLOAD	0xEF08A53C
	#define CONFIG_ROCKCHIP_BOOT_MODE_REG 0xfd588080
#else
	#include <asm/arch/boot_mode.h>
#endif // CONFIG_SANDBOX

#undef DEBUG_SYS_UPDATE

#ifdef DEBUG_SYS_UPDATE
#define __msg(fmt,args...) printf(fmt,##args)
#else
#define __msg(fmt,args...)
#endif

#ifndef DWORD
typedef unsigned int DWORD;
#endif
#ifndef BYTE
typedef unsigned char BYTE;
#endif

extern int SM3_hash(BYTE *msg, DWORD len1, DWORD *out_hash);
extern void out_hex(DWORD *list1);

#define PPC_SWAP32(x) ((((x) & 0x000000ff) << 24) | (((x) & 0x0000ff00) << 8)|\
 		  (((x) & 0x00ff0000) >>  8) | (((x) & 0xff000000) >> 24) )
			  		  
static unsigned int *crc32_table = NULL;

unsigned int* crc32_filltable(unsigned int *crc_table, int endian)
{
	unsigned int polynomial = endian ? 0x04c11db7 : 0xedb88320;
	unsigned int c;
	int i, j;

	if (!crc_table)
		crc_table = malloc(256 * sizeof(unsigned int));

	for (i = 0; i < 256; i++) {
		c = endian ? (i << 24) : i;
		for (j = 8; j; j--) {
			if (endian)
				c = (c&0x80000000) ? ((c << 1) ^ polynomial) : (c << 1);
			else
				c = (c&1) ? ((c >> 1) ^ polynomial) : (c >> 1);
		}
		*crc_table++ = c;
	}

	return crc_table - 256;
}

/*
 * cksum - calculate the CRC32 checksum of a file
 *
 * Copyright (C) 2006 by Rob Sullivan, with ideas from code by Walter Harms
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details. */
int cksum_file(unsigned char * buf, unsigned long length)
{
	unsigned int crc;
	unsigned char *cp = buf;
	unsigned long filesize;
	
	crc = 0;
	filesize = length;
	
	cp = (unsigned char *) buf;
	do {
		crc = (crc << 8) ^ crc32_table[(crc >> 24) ^ *cp++];
	} while (--filesize);

	while (length) {
		crc = (crc << 8) ^ crc32_table[(unsigned char)(crc >> 24) ^ (unsigned char)length];
		/* must ensure that shift is unsigned! */
		if (sizeof(length) <= sizeof(unsigned))
			length = (unsigned)length >> 8;
		else if (sizeof(length) <= sizeof(unsigned long))
			length = (unsigned long)length >> 8;
		else
			length = (unsigned long long)length >> 8;
	}
	crc = ~crc;

	__msg("Calculate CRC: %08x\n", crc);

	return crc;
}

#define CHECK_HEAD(x)  (((uint8_t)((x) >> 24) & 0xff) == 'S') && \
					   (((uint8_t)((x) >> 16) & 0xff) == 'K') && \
					   (((uint8_t)((x) >> 8) & 0xff) == 'Y') && \
					   (((uint8_t)((x)) & 0xff) == 'S')

#define CHECK_TAIL(x)  (((uint8_t)((x) >> 24) & 0xff) == 'T') && \
					   (((uint8_t)((x) >> 16) & 0xff) == 'E') && \
					   (((uint8_t)((x) >> 8) & 0xff) == 'C') && \
					   (((uint8_t)((x)) & 0xff) == 'H')

