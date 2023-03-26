/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include "veeprom.h"

#define MMCBLK		"/dev/mmcblk0"

int fd;
int bootmode = 0;

static void help(void)
{
	printf("Usage:  hrut_ddr_freq [OPTIONS] [Values]\n");
	printf("Example: \n");
	printf("       hrut_ddr_freq g\n");
	printf("       hrut_ddr_freq s 2666\n");
	printf("Options:\n");
	printf("       g   get ddr frequency(bootinfo)\n");
	printf("       s   set ddr frequency(bootinfo)\n");
	printf("       h   display this help text\n");
	printf("Values:\n");
	printf("       [ 2133|2666|3200|4266]\n");
}

static unsigned int get_ddr_freq(unsigned int boardid)
{
	unsigned int ddr_freq = (boardid >> 20) & 0xf;
	unsigned int ddr_freq_value = 0;

	switch (ddr_freq) {
	case DDR_FREQC_2133:
		ddr_freq_value = 2133;
		break;
	case DDR_FREQC_2666:
		ddr_freq_value = 2666;
		break;
	case DDR_FREQC_3200:
		ddr_freq_value = 3200;
		break;
	case DDR_FREQC_4266:
		ddr_freq_value = 4266;
		break;
	case DDR_FREQC_3600:
		ddr_freq_value = 3600;
		break;
	default:
		ddr_freq_value = 0;
		break;
	}

	return ddr_freq_value;
}

static unsigned int set_ddr_freq(unsigned int boardid, unsigned int ddr_freq)
{
	unsigned int ddr_freq_value = 0;

	switch (ddr_freq) {
	case 2133:
		ddr_freq_value = DDR_FREQC_2133;
		break;
	case 2666:
		ddr_freq_value = DDR_FREQC_2666;
		break;
	case 3200:
		ddr_freq_value = DDR_FREQC_3200;
		break;
	case 4266:
		ddr_freq_value = DDR_FREQC_4266;
		break;
	case 3600:
		ddr_freq_value = DDR_FREQC_3600;
		break;
	default:
		ddr_freq_value = 0;
		break;
	}

	boardid = boardid & (~(0xf << 20));
	boardid = boardid | (ddr_freq_value << 20);

	return boardid;
}

static bool check_ddr_freq(unsigned int ddr_freq)
{
	if (ddr_freq == 2133 || ddr_freq == 2666 || ddr_freq == 3200 || \
		ddr_freq == 4266 || ddr_freq == 0 || ddr_freq == 3600) {
		return true;
	} else {
		return false;
	}
}

static int get_nor_ddr_freq_bootinfo(void)
{
	int ddr_freq = 0;
	char mbr_buf[512] = {0};
	unsigned int mbr_sector_left = 128;
	unsigned int *p_board_id = NULL;
	size_t ret = 0;

	fd = open("/dev/mtd1", O_RDONLY);
	if (fd < 0) {
		printf("Error: open /dev/mtd1  fail!\n");
		return -1;
	}

	ret = read(fd, mbr_buf, sizeof(mbr_buf));
	if (ret < sizeof(mbr_buf)) {
		printf("Error: read sector %d fail\n", mbr_sector_left);
		close(fd);
		return -1;
	}

	/* ddr freq offset: 0xc4 size: 4 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
	ddr_freq = get_ddr_freq(*p_board_id);
	close(fd);

	printf("%d\n", ddr_freq);

	return ddr_freq;
}

static int get_nand_ddr_freq_bootinfo(void)
{
	int ddr_freq = 0;
	char mbr_buf[512] = {0};
	unsigned int *p_board_id = NULL;
	ssize_t ret = 0;

	fd = open("/dev/mtd0", O_RDONLY);
	if (fd < 0) {
		printf("Error: open /dev/mtd0  fail\n");
		return -1;
	}

	ret = read(fd, mbr_buf, sizeof(mbr_buf));
	if (ret < sizeof(mbr_buf)) {
		printf("Error: read from nand fail\n");
		close(fd);
		return -1;
	}

	/* ddr freq offset: 0xc4 size: 4 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
	ddr_freq = get_ddr_freq(*p_board_id);

	printf("%d\n", ddr_freq);
	close(fd);

	return ddr_freq;
}

static unsigned int get_mmc_ddr_freq_bootinfo(void)
{
	int ddr_freq = 0;
	FILE *fp = NULL;
	char mbr_buf[512] = {0};
	unsigned int *p_board_id = NULL;

	if ((fp = fopen(MMCBLK, "rb")) == NULL) {
		printf("open %s error\n", MMCBLK);
		exit(1);
	}
	if (fread(mbr_buf, 1, 512, fp) != 512) {
		printf("read %s error\n", MMCBLK);
		fclose(fp);
		exit(1);
	}
	fclose(fp);

	/* ddr freq offset: 0xc0 size: 4 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);

	ddr_freq = get_ddr_freq(*p_board_id);

	printf("%d\n", ddr_freq);

	return ddr_freq;
}

static void set_ddr_freq_bootinfo(unsigned int ddr_freq)
{
	unsigned int ddr_freq_old = 0;
	unsigned int board_id_new = 0;
	FILE *fp = NULL;
	char mbr_buf[512] = {0};
	int i = 0;
	int sum = 0;
	int *p_sum = NULL, *p_board_id = NULL;

	if ((fp = fopen(MMCBLK, "r+b")) == NULL) {
		printf("open %s error\n", MMCBLK);
		exit(1);
	}

	if (fread(mbr_buf, 1, 512, fp) != 512) {
		printf("read %s error\n", MMCBLK);
		fclose(fp);
		exit(1);
	}

	/* bootinfo check sum offset: 0xc */
	p_sum = (int *)(mbr_buf + CHECK_SUM_OFFSET);
	*p_sum = 0;

	/* ddr freq offset: 0xc4 size: 4 */
	p_board_id = (int *)(mbr_buf + BOARD_ID_OFFSET);
	ddr_freq_old = get_ddr_freq(*p_board_id);

	if (ddr_freq_old == ddr_freq) {
		printf("the ddr_freq is same to the old\n");
		fclose(fp);
		exit(0);
	}

	board_id_new = set_ddr_freq(*p_board_id, ddr_freq);
	*p_board_id = board_id_new;

	for (i = 0; i < 275; i++)
		sum += mbr_buf[i] & 0xff;
	*p_sum = sum;

	fseek(fp, 0, SEEK_SET);
	if (fwrite(mbr_buf, 1, 512, fp) != 512) {
		printf("write %s error\n", MMCBLK);
		fclose(fp);
		exit(1);
	}

	fclose(fp);

	printf("change ddr freq from %d to %d\n", ddr_freq_old, ddr_freq);
}

int main(int argc, char **argv)
{
	unsigned int data;

	if (argc < 2) {
		help();
		return -1;
	}

	bootmode = get_boot_mode();

	switch(argv[1][0]) {
	case 'g':
		if (bootmode == PIN_2ND_SPINOR)
			get_nor_ddr_freq_bootinfo();
		else if (bootmode == PIN_2ND_SPINAND)
			get_nand_ddr_freq_bootinfo();
		else
			get_mmc_ddr_freq_bootinfo();
		break;
	case 's':
		if (argc < 3) {
			printf("error: too less parameter\n");
			break;
		}
		data = atoi(argv[2]);

		if(!check_ddr_freq(data)) {
			printf("Error: invalid ddr freq %s !\n", argv[2]);
			break;
		}

		if (bootmode == PIN_2ND_SPINOR || bootmode == PIN_2ND_SPINAND)
			printf("error: not support set bootinfo's ddr_freq in flashes\n");
		else
			set_ddr_freq_bootinfo(data);
		break;
	case 'h':
		help();
		break;
	default:
		help();
		break;
	}

	return 0;
}
