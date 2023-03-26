/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include "veeprom.h"

#define MMCBLK		"/dev/mmcblk0"

extern int fd;
extern char bootmode;

void help(void)
{
	printf("Usage:  hrut_boardid [OPTIONS] <Values>\n");
	printf("Example: \n");
	printf("       hrut_boardid g\n");
	printf("Options:\n");
	printf("       g   get board id(veeprom)\n");
	printf("       s   set board id(veeprom)\n");
	printf("       G   get board id(bootinfo)\n");
	printf("       S   set board id(bootinfo)\n");
	printf("       c   clear board id(veeprom)\n");
	printf("       C   clear board id(bootinfo)\n");
	printf("       h   display this help text\n");
}

long hb_str2hex(const char *str)
{
        char *endptr;
        long val = 0;
        errno = 0;

	if (str == NULL) {
		printf("input parameter is null\n");
		return -1;
	}
        val = strtol(str, &endptr, 16);
        if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) {
                perror("strtol");
                return -1;
        }
        if (endptr == str) {
                printf("No digits were found\n");
                return -1;
        }
        if (*endptr != '\0') {
                printf("invalid input parameter:%s\n", str);
                return -1;
        }
        return val;

}

static int get_board_id_from_sysfs(void)
{
	int fd;
	ssize_t ret = 0;
	long board_id;
	char buf[32] = {0};

	fd = open("/sys/class/socinfo/board_id", O_RDONLY);
	if(fd == -1) {
		printf("boardid fd open error\n");
		return -1;
	}
	ret = read(fd, buf, 8);
	if (ret < 1) {
		printf("read /sys/class/socinfo/boardid failed\n");
		close(fd);
		return -1;
	}
	close(fd);
	errno = 0;
	board_id = hb_str2hex(buf);

	return (int)board_id;
}

static unsigned int get_nor_board_id_bootinfo(void)
{
	int board_id = 0;
	char mbr_buf[512] = {0};
	unsigned int mbr_sector_left = 128;
	unsigned int *p_board_id = NULL;
	ssize_t ret = 0;

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

	/* boardid offset: 0xc0 size: 4 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
	board_id = *p_board_id;
	close(fd);

	printf("%02x\n", board_id);

	return board_id;
}

static unsigned int get_nand_board_id_bootinfo(void)
{
	int board_id = 0;
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
		return -1;
	}

	/* boardid offset: 0xc0 size: 4 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
	board_id = *p_board_id;

	printf("%02x\n", board_id);

	return board_id;
}

static unsigned int get_mmc_board_id_bootinfo(void)
{
	int board_id = 0;
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

	/* boardid offset: 0xc0 size: 4 */
	p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
	board_id = *p_board_id;

	printf("%02x\n", board_id);

	return board_id;
}

static void set_board_id_bootinfo(long board_id)
{
	int board_id_old = 0;
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

	/* 0x000c 512bootinfo info_csum */
	p_sum = (int *)(mbr_buf + CHECK_SUM_OFFSET);
	*p_sum = 0;

	/* boardid offset: 0xc0 size: 4 */
	p_board_id = (int *)(mbr_buf + BOARD_ID_OFFSET);
	board_id_old = *p_board_id;
	mbr_buf[0xc0] = (char)(board_id & 0xff);
	mbr_buf[0xc1] = (char)((board_id >> 8) & 0xff);
	mbr_buf[0xc2] = (char)((board_id >> 16) & 0xff);
	mbr_buf[0xc3] = (char)((board_id >> 24) & 0xff);

	if (board_id_old == board_id) {
		printf("\nthe board_id is same to the old\n");
		fclose(fp);
		exit(0);
	}

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

	printf("change board id from %x to %lx\n", board_id_old,
			(unsigned long int)board_id);
}

int main(int argc, char **argv)
{
	char flag[6] = {0};
	int ret;
	char *addr;
	long data;
	int boardid;

	if (argc < 2) {
		help();
		return -1;
	}

	if (veeprom_init() < 0) {
		printf("veeprom_init error\n");
		return -1;
	}

	switch(argv[1][0]) {
	case 'g':
		if(argc != 2) {
			printf("hrut_boardid: parameter not support -- 'g'\n");
			help();
			break;
		}

		if ((ret = veeprom_read(VEEPROM_X3_BOARD_ID_OFFSET,
			&flag[0], VEEPROM_X3_BOARD_ID_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		data = (flag[0] << 24) + (flag[1] << 16) + (flag[2] << 8) + flag[3];
		boardid = get_board_id_from_sysfs();
		if (boardid == -1) {
			veeprom_exit();
			printf("get board id from sysfs failed\n");
			return -1;
		}
		if(data != 0x0000) {
			if (data != boardid) {
				/* set boardid */
				flag[0] = (char)((boardid >> 24) & 0xff);
				flag[1] = (char)((boardid >> 16) & 0xff);
				flag[2] = (char)((boardid >> 8) & 0xff);
				flag[3] = (char)(boardid & 0xff);
				if ((ret = veeprom_write(VEEPROM_X3_BOARD_ID_OFFSET,
					&flag[0], VEEPROM_X3_BOARD_ID_SIZE) < 0)) {
					printf("saving board operation ret = %d\n", ret);
					veeprom_exit();
					return -1;
				}
			}
			printf("%02x\n", boardid);
		} else {
			printf("not set veeprom boardid yet, setting value from sysfs!\n");
			printf("boardid = %x\n", boardid);

			/* set boardid */
			flag[0] = (char)((boardid >> 24) & 0xff);
			flag[1] = (char)((boardid >> 16) & 0xff);
			flag[2] = (char)((boardid >> 8) & 0xff);
			flag[3] = (char)(boardid & 0xff);
			if ((ret = veeprom_write(VEEPROM_X3_BOARD_ID_OFFSET,
				&flag[0], VEEPROM_X3_BOARD_ID_SIZE) < 0)) {
				printf("saving board operation ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}
		}
		break;
	case 's':
		if (argc != 3) {
			printf("hrut_boardid: parameter not support -- 's'\n");
			help();
			break;
		}
		addr = argv[2];

		if (strlen(addr) > 8) {
			printf("Error: invalid board id %s, do not save value" \
				" from user input \n", addr);
			help();
			return -1;
		}

		data = hb_str2hex(addr);
		if (data == -1) {
			printf("transfer %s to int failed\n", addr);
			veeprom_exit();
			return -1;
		}
		flag[0] = (char)((data >> 24) & 0xff);
		flag[1] = (char)((data >> 16) & 0xff);
		flag[2] = (char)((data >> 8) & 0xff);
		flag[3] = (char)(data & 0xff);
		printf("set board id = %s \n", addr);
		fflush(stdout);

		if ((ret = veeprom_write(VEEPROM_X3_BOARD_ID_OFFSET,
			&flag[0], VEEPROM_X3_BOARD_ID_SIZE) < 0)) {
			printf("saving board operation ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		break;
	case 'G':
		if(argc != 2) {
			printf("hrut_boardid: parameter not support -- 'G'\n");
			help();
			break;
		}

		if (bootmode == PIN_2ND_SPINOR)
			get_nor_board_id_bootinfo();
		else if (bootmode == PIN_2ND_SPINAND)
			get_nand_board_id_bootinfo();
		else
			get_mmc_board_id_bootinfo();

		break;
	case 'S':
		if (argc != 3) {
			printf("hrut_boardid: parameter not support -- 'S'\n");
			help();
			break;
		}
		addr = argv[2];

		if (strlen(addr) > 8) {
			printf("Error: invalid board id %s, do not save value" \
				" from user input \n", addr);
			help();
			return -1;
		}

		data = hb_str2hex(addr);
		if (data == -1) {
			printf("transfer %s to int failed\n", addr);
			veeprom_exit();
			return -1;
		}

		if (bootmode == PIN_2ND_SPINOR || bootmode == PIN_2ND_SPINAND)
			printf("error: not support set bootinfo's board id in flashes\n");
		else
			set_board_id_bootinfo(data);

		break;
	case 'c':
		if(argc != 2) {
			printf("hrut_boardid: parameter not support -- 'c'\n");
			help();
			break;
		}

		flag[0] = 0;
		flag[1] = 0;
		if ((ret = veeprom_write(VEEPROM_X3_BOARD_ID_OFFSET,
			&flag[0], VEEPROM_X3_BOARD_ID_SIZE) < 0)) {
			printf("clear boardid operation ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		printf("clear boardid success!\n");
		break;
	case 'C':
		if(argc != 2) {
			printf("hrut_boardid: parameter not support -- 'C'\n");
			help();
			break;
		}

		data = 0;
		if (bootmode == PIN_2ND_SPINOR || bootmode == PIN_2ND_SPINAND)
			printf("error: not support clear bootinfo's board id in flashes\n");
		else
			set_board_id_bootinfo(data);

		printf("clear bootinfo boardid success!\n");
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
