/*
 * (C) Copyright 2003
 * Kyle Harris, kharris@nexus-tech.net
 *
 * SPDX-License-Identifier:	GPL-2.0+
 * backport2025 
 * rm old mmc commands, only keep rpmb code
 */
#include <common.h>
#include <command.h>
#include <console.h>
#include <mmc.h>
#include <optee_include/OpteeClientInterface.h>
#include <optee_include/OpteeClientApiLib.h>
#include <optee_test.h>

static int curr_device = -1;


#ifdef CONFIG_OPTEE_CLIENT
static int do_mmc_test_secure_storage(cmd_tbl_t *cmdtp,
				      int flag, int argc, char * const argv[])
{
	struct mmc *mmc;

	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			puts("MMC device available\n");
			curr_device = 0;
		} else {
			puts("No MMC device available\n");
			return 1;
		}
	}

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	int i, count = 100;

	for (i = 1; i <= count; i++) {
		if (test_secure_storage_default() == 0) {
			printf("test_secure_storage_default success! %d/%d\n", i, count);
		} else {
			printf("test_secure_storage_default fail! %d/%d\n", i, count);
			break;
		}
		if (test_secure_storage_security_partition() == 0) {
			printf("test_secure_storage_security_partition success! %d/%d\n", i, count);
		} else {
			printf("test_secure_storage_security_partition fail! %d/%d\n", i, count);
			break;
		}
	}

	return CMD_RET_SUCCESS;
}

static int do_mmc_testefuse(cmd_tbl_t *cmdtp,
		int flag, int argc, char * const argv[])
{
	uint32_t outbuf32[8];

	trusty_read_attribute_hash(outbuf32, 8);

	printf(" 0x%x  0x%x  0x%x  0x%x \n",
		outbuf32[0], outbuf32[1], outbuf32[2], outbuf32[3]);
	printf(" 0x%x  0x%x  0x%x  0x%x \n",
		outbuf32[4], outbuf32[5], outbuf32[6], outbuf32[7]);

	return CMD_RET_SUCCESS;
}

#endif

#ifdef CONFIG_SUPPORT_EMMC_RPMB
char temp_original_part;
int init_rpmb(void)
{
	struct mmc *mmc;

	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			curr_device = 0;
		} else {
			printf("No MMC device available\n");
			return CMD_RET_FAILURE;
		}
	}

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (!(mmc->version & MMC_VERSION_MMC)) {
		printf("It is not a EMMC device\n");
		return CMD_RET_FAILURE;
	}
	if (mmc->version < MMC_VERSION_4_41) {
		printf("RPMB not supported before version 4.41\n");
		return CMD_RET_FAILURE;
	}

		/* Switch to the RPMB partition */
#ifndef CONFIG_BLK
	temp_original_part = mmc->block_dev.hwpart;
	debug("mmc->block_dev.hwpart\n");
#else
	temp_original_part = mmc_get_blk_desc(mmc)->hwpart;
	debug("mmc_get_blk_desc(mmc)->hwpart\n");
#endif
	debug("init_rpmb temp_original_part = 0x%X\n", temp_original_part);
	if (blk_select_hwpart_devnum(UCLASS_MMC/*IF_TYPE_MMC*/, curr_device, MMC_PART_RPMB) != 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

int finish_rpmb(void)
{
	/* Return to original partition */
	debug("finish_rpmb temp_original_part = 0x%X\n", temp_original_part);
	if (blk_select_hwpart_devnum(UCLASS_MMC/*IF_TYPE_MMC*/, curr_device, temp_original_part) != 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

int do_readcounter(struct s_rpmb *requestpackets)
{
	struct mmc *mmc = find_mmc_device(curr_device);

	return read_counter(mmc, requestpackets);
}

int do_programkey(struct s_rpmb *requestpackets)
{
	struct mmc *mmc = find_mmc_device(curr_device);

	return program_key(mmc, requestpackets);
}

int do_authenticatedread(struct s_rpmb *requestpackets, uint16_t block_count)
{
	struct mmc *mmc = find_mmc_device(curr_device);

	return authenticated_read(mmc, requestpackets, block_count);
}

int do_authenticatedwrite(struct s_rpmb *requestpackets)
{
	struct mmc *mmc = find_mmc_device(curr_device);

	return authenticated_write(mmc, requestpackets);
}

struct mmc *do_returnmmc(void)
{
	struct mmc *mmc = find_mmc_device(curr_device);

	return mmc;
}

static int confirm_key_prog(void)
{
	puts("Warning: Programming authentication key can be done only once !\n"
	     "         Use this command only if you are sure of what you are doing,\n"
	     "Really perform the key programming? <y/N> ");
	if (confirm_yesno())
		return 1;

	puts("Authentication key programming aborted\n");
	return 0;
}
static int do_mmcrpmb_key(cmd_tbl_t *cmdtp, int flag,
			  int argc, char * const argv[])
{
	void *key_addr;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (argc != 2)
		return CMD_RET_USAGE;

	key_addr = (void *)simple_strtoul(argv[1], NULL, 16);
	if (!confirm_key_prog())
		return CMD_RET_FAILURE;
	if (mmc_rpmb_set_key(mmc, key_addr)) {
		printf("ERROR - Key already programmed ?\n");
		return CMD_RET_FAILURE;
	}
	return CMD_RET_SUCCESS;
}
static int do_mmcrpmb_read(cmd_tbl_t *cmdtp, int flag,
			   int argc, char * const argv[])
{
	u16 blk, cnt;
	void *addr;
	int n;
	void *key_addr = NULL;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (argc < 4)
		return CMD_RET_USAGE;

	addr = (void *)simple_strtoul(argv[1], NULL, 16);
	blk = simple_strtoul(argv[2], NULL, 16);
	cnt = simple_strtoul(argv[3], NULL, 16);

	if (argc == 5)
		key_addr = (void *)simple_strtoul(argv[4], NULL, 16);

	printf("\nMMC RPMB read: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);
	n =  mmc_rpmb_read(mmc, addr, blk, cnt, key_addr);

	printf("%d RPMB blocks read: %s\n", n, (n == cnt) ? "OK" : "ERROR");
	if (n != cnt)
		return CMD_RET_FAILURE;
	return CMD_RET_SUCCESS;
}
static int do_mmcrpmb_write(cmd_tbl_t *cmdtp, int flag,
			    int argc, char * const argv[])
{
	u16 blk, cnt;
	void *addr;
	int n;
	void *key_addr;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (argc != 5)
		return CMD_RET_USAGE;

	addr = (void *)simple_strtoul(argv[1], NULL, 16);
	blk = simple_strtoul(argv[2], NULL, 16);
	cnt = simple_strtoul(argv[3], NULL, 16);
	key_addr = (void *)simple_strtoul(argv[4], NULL, 16);

	printf("\nMMC RPMB write: dev # %d, block # %d, count %d ... ",
	       curr_device, blk, cnt);
	n =  mmc_rpmb_write(mmc, addr, blk, cnt, key_addr);

	printf("%d RPMB blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");
	if (n != cnt)
		return CMD_RET_FAILURE;
	return CMD_RET_SUCCESS;
}
static int do_mmcrpmb_counter(cmd_tbl_t *cmdtp, int flag,
			      int argc, char * const argv[])
{
	unsigned long counter;
	struct mmc *mmc = find_mmc_device(curr_device);

	if (mmc_rpmb_get_counter(mmc, &counter))
		return CMD_RET_FAILURE;
	printf("RPMB Write counter= %lx\n", counter);
	return CMD_RET_SUCCESS;
}

static cmd_tbl_t cmd_rpmb[] = {
	U_BOOT_CMD_MKENT(key, 2, 0, do_mmcrpmb_key, "", ""),
	U_BOOT_CMD_MKENT(read, 5, 1, do_mmcrpmb_read, "", ""),
	U_BOOT_CMD_MKENT(write, 5, 0, do_mmcrpmb_write, "", ""),
	U_BOOT_CMD_MKENT(counter, 1, 1, do_mmcrpmb_counter, "", ""),
};

static int do_mmcrpmb(cmd_tbl_t *cmdtp, int flag,
		      int argc, char * const argv[])
{
	cmd_tbl_t *cp;
	struct mmc *mmc;
	char original_part;
	int ret;

	cp = find_cmd_tbl(argv[1], cmd_rpmb, ARRAY_SIZE(cmd_rpmb));

	/* Drop the rpmb subcommand */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cp->repeatable)
		return CMD_RET_SUCCESS;

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	if (!(mmc->version & MMC_VERSION_MMC)) {
		printf("It is not a EMMC device\n");
		return CMD_RET_FAILURE;
	}
	if (mmc->version < MMC_VERSION_4_41) {
		printf("RPMB not supported before version 4.41\n");
		return CMD_RET_FAILURE;
	}
	/* Switch to the RPMB partition */
#ifndef CONFIG_BLK
	original_part = mmc->block_dev.hwpart;
#else
	original_part = mmc_get_blk_desc(mmc)->hwpart;
#endif
	if (blk_select_hwpart_devnum(UCLASS_MMC/*IF_TYPE_MMC*/, curr_device, MMC_PART_RPMB) !=
	    0)
		return CMD_RET_FAILURE;
	ret = cp->cmd(cmdtp, flag, argc, argv);

	/* Return to original partition */
	if (blk_select_hwpart_devnum(UCLASS_MMC/*IF_TYPE_MMC*/, curr_device, original_part) !=
	    0)
		return CMD_RET_FAILURE;
	return ret;
}
#endif


#if 0
#ifdef CONFIG_OPTEE_CLIENT
	U_BOOT_CMD_MKENT(testsecurestorage, 1, 0, do_mmc_test_secure_storage, "", ""),
	U_BOOT_CMD_MKENT(testefuse, 1, 0, do_mmc_testefuse, "", ""),
#endif
#ifdef CONFIG_SUPPORT_EMMC_RPMB
	U_BOOT_CMD_MKENT(rpmb, CONFIG_SYS_MAXARGS, 1, do_mmcrpmb, "", ""),
#endif
	U_BOOT_CMD_MKENT(setdsr, 2, 0, do_mmc_setdsr, "", ""),
#ifdef CONFIG_CMD_BKOPS_ENABLE
	U_BOOT_CMD_MKENT(bkops-enable, 2, 0, do_mmc_bkops_enable, "", ""),
#endif
};
#ifdef CONFIG_OPTEE_CLIENT
	"mmc testsecurestorage - test CA call static TA to store data in security\n"
	"mmc testefuse - test CA call static TA,and TA read or write efuse\n"
#endif
#ifdef CONFIG_SUPPORT_EMMC_RPMB
	"mmc rpmb read addr blk# cnt [address of auth-key] - block size is 256 bytes\n"
	"mmc rpmb write addr blk# cnt <address of auth-key> - block size is 256 bytes\n"
	"mmc rpmb key <address of auth-key> - program the RPMB authentication key.\n"
	"mmc rpmb counter - read the value of the write counter\n"
#endif
	"mmc setdsr <value> - set DSR register value\n"
#ifdef CONFIG_CMD_BKOPS_ENABLE
	"mmc bkops-enable <dev> - enable background operations handshake on device\n"
	"   WARNING: This is a write-once setting.\n"
#endif
	);

/* Old command kept for compatibility. Same as 'mmc info' */
U_BOOT_CMD(
	mmcinfo, 1, 0, do_mmcinfo,
	"display MMC info",
	"- display info of the current MMC device"
);
#endif 