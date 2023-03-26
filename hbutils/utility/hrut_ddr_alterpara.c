/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "veeprom.h"

#define MMCBLK		"/dev/mmcblk0"

int fd;
int bootmode;

static void help(void)
{
	printf("Usage:  hrut_ddr_alterpara [OPTIONS] [Values]\n");
	printf("Example: \n");
	printf("       hrut_ddr_alterpara g\n");
	printf("       hrut_ddr_alterpara s on\n");
	printf("Options:\n");
	printf("       g   get ddr alterpara config(bootinfo)\n");
	printf("       s   set ddr alterpara config(bootinfo)\n");
	printf("       h   display this help text\n");
	printf("Values:\n");
	printf("       [ on|off ], defaut off\n");
}

static unsigned int get_nor_ddr_alter_config(void)
{
	int alter_para = 0;
	char mbr_buf[512] = {0};
	unsigned int mbr_sector_left = 128;
	unsigned int p_board_id = 0;

	if (lseek(fd, mbr_sector_left * 512, SEEK_SET) < 0) {
		printf("Error: lseek sector %d fail\n", mbr_sector_left);
		return -1;
	}

	if (read(fd, mbr_buf, sizeof(mbr_buf)) < 0) {
		printf("Error: read sector %d fail\n", mbr_sector_left);
		return -1;
	}

	/* board id offset: 0xc0 */
	p_board_id = mbr_buf[BOARD_ID_OFFSET];
	alter_para = (p_board_id >> 4) & 0xf;

	if (alter_para == 1)
		printf("on\n");
	else if (alter_para == 0)
		printf("off\n");
	else
		printf("Config not support!\n");

	return alter_para;
}

static unsigned int get_nand_ddr_alter_config(void)
{
	int alter_para = 0;
	char mbr_buf[512] = {0};
	unsigned int p_board_id = 0;

	if (read(fd, mbr_buf, sizeof(mbr_buf)) < 0) {
		printf("Error: read from nand fail\n");
		return -1;
	}

	/* board id offset: 0xc0 */
	p_board_id = mbr_buf[BOARD_ID_OFFSET];
	alter_para = (p_board_id >> 4) & 0xf;

	if (alter_para == 1)
		printf("on\n");
	else if (alter_para == 0)
		printf("off\n");
	else
		printf("Config not support!\n");

	return alter_para;
}

static unsigned int get_mmc_ddr_alter_config(void)
{
	int alter_para = 0;
	FILE *fp = NULL;
	char mbr_buf[512] = {0};
	unsigned int *p_board_id = NULL;

	if ((fp = fopen(MMCBLK, "rb")) == NULL) {
		printf("open %s error\n", MMCBLK);
		exit(1);
	}
	if (fread(mbr_buf, 1, 512, fp) != 512) {
		printf("read %s error\n", MMCBLK);
		exit(1);
	}
	fclose(fp);

	/* board id offset: 0xc0 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
	alter_para = ((*p_board_id) >> 4) & 0xf;

	if (alter_para == 1)
		printf("on\n");
	else if (alter_para == 0)
		printf("off\n");
	else
		printf("Config not support!\n");

	return alter_para;
}

static void set_ddr_alter_config(unsigned int alter_para)
{
	unsigned int ddr_alter_para_old = 0;
	unsigned int board_id_new = 0;
	unsigned int *p_board_id = NULL;
	FILE *fp = NULL;
	char mbr_buf[512] = {0};
	int i = 0, sum = 0;
	int *p_sum = NULL;

	if ((fp = fopen(MMCBLK, "r+b")) == NULL) {
		printf("open %s error\n", MMCBLK);
		exit(1);
	}

	if (fread(mbr_buf, 1, 512, fp) != 512) {
		printf("read %s error\n", MMCBLK);
		exit(1);
	}

	/* bootinfo check sum offset: 0xc */
	p_sum = (int *)(mbr_buf + CHECK_SUM_OFFSET);
	*p_sum = 0;

	/* board id offset: 0xc0 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
	ddr_alter_para_old = ((*p_board_id) >> 4) & 0xf;

	if (ddr_alter_para_old == alter_para) {
		printf("the ddr_alter config is same to the old\n");
		fclose(fp);
		exit(0);
	}

	if (alter_para == 1)
		board_id_new = ((*p_board_id) & (~(0xf << 4))) | (0x1 << 4);
	else if (alter_para == 0)
		board_id_new = (*p_board_id) & (~(0xf << 4));
	mbr_buf[BOARD_ID_OFFSET] = (char)board_id_new;

	for (i = 0; i < 275; i++)
		sum += mbr_buf[i] & 0xff;
	*p_sum = sum;

	fseek(fp, 0, SEEK_SET);
	if (fwrite(mbr_buf, 1, 512, fp) != 512) {
		printf("write %s error\n", MMCBLK);
		exit(1);
	}

	fclose(fp);

	printf("change ddr_alter_config from %d to %d\n", ddr_alter_para_old,
		alter_para);
}

int main(int argc, char **argv)
{
	char data[32] = { 0 };
	unsigned int alter_para = 0;

	if (argc < 2) {
		help();
		return -1;
	}

	bootmode = get_boot_mode();

	switch(argv[1][0]) {
	case 'g':
		if(argc != 2) {
			help();
			break;
		}

		if (bootmode == PIN_2ND_SPINOR)
			get_nor_ddr_alter_config();
		else if (bootmode == PIN_2ND_SPINAND)
			get_nand_ddr_alter_config();
		else
			get_mmc_ddr_alter_config();
		break;
	case 's':
		if (argc != 3) {
			printf("hrut_ddr_alterpara: option requires an argument -- 's'\n");
			help();
			break;
		}
		memset(data, 0, sizeof(data));
		strcpy(data, argv[2]);

		if ((strcmp(data, "on") != 0) && (strcmp(data, "off") != 0)) {
			printf("Error: invalid argument %s !\n", argv[2]);
			break;
		}

		if (strcmp(data, "on") == 0)
			alter_para = 1;

		if (bootmode == PIN_2ND_SPINOR || bootmode == PIN_2ND_SPINAND)
			printf("Error: not support set bootinfo's ddr_alterconfig in flashes\n");
		else
			set_ddr_alter_config(alter_para);
		break;
	case 'h':
		help();
		break;
	default:
		help();
		break;
	}

	veeprom_exit();

	return 0;
}
