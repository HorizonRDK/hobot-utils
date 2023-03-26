/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "veeprom.h"

void help(void)
{
	printf("Usage: hrut_logmask [Options] <Value>\n");
	printf("Example: \n");
	printf("       hrut_logmask g\n");
	printf("       hrut_logmask s verbose\n");
	printf("Options:\n");
	printf("       g           get log level\n");
	printf("       s           set log level\n");
	printf("       c           clean log level\n");
	printf("       h           display this help text\n");
	printf("Value:\n");
	printf("       verbose     very detaild information, intended only for development\n");
	printf("       debug       developer stuff\n");
	printf("       info        important business process has finish\n");
	printf("       warn        the process might be continued, but take extra caution\n");
	printf("       error       something terribly wrong had happened, the must be investigated immediately\n");
	
}

static int check_logmask_para(char *c, int len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		if (!isalnum(c[i]))
			return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	char flag;
	char data[16] = { 0 };
	char data2[16] = { 0 };
	int ret;

	if(argc < 2) {
		help();
		return -1;
	}

	if (veeprom_init() < 0) {
		printf("veeprom_init error\n");
		return -1;
	}

	flag = *argv[1];

	switch (flag) {
	case 'g':
		if ((ret = veeprom_read(VEEPROM_LOG_MASK_OFFSET, data2,
				VEEPROM_LOG_MASK_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		printf("%s\n", data2);
		break;
	case 's':
		if ((argc < 3)) {
			help();
			break;
		}

		memset(data, 0, sizeof(data));
		strcpy(data, argv[2]);

		if (strlen(data) > VEEPROM_LOG_MASK_SIZE) {
			help();
			break;
		}

		if (check_logmask_para(data, (int)strlen(data))) {
			help();
			break;
		}

		if ((ret = veeprom_write(VEEPROM_LOG_MASK_OFFSET, data,
				VEEPROM_LOG_MASK_SIZE)) < 0) {
			printf("veeprom_write ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		if ((ret = veeprom_read(VEEPROM_LOG_MASK_OFFSET, data2,
				VEEPROM_LOG_MASK_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		if(strcmp(data2, data) != 0) {
			printf("verify failed\n");
			veeprom_exit();
			return -1;
		}

		printf("%s\n", data2);
		break;
	case 'c':
		memset(data, 0, sizeof(data));

		if ((ret = veeprom_write(VEEPROM_LOG_MASK_OFFSET, data,
				VEEPROM_LOG_MASK_SIZE)) < 0) {
			printf("veeprom_write ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		break;
	case 'h':
		help();
		break;

	default:
		help();
		return -1;
	};

	veeprom_exit();

	return 0;
}
