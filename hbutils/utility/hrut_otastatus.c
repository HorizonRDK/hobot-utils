/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "veeprom.h"

void help(void)
{
	printf("Usage: hrut_otastatus [Options] <Target> <value>\n");
	printf("Example: \n");
	printf("       hrut_otastatus g upflag\n");
	printf("       hrut_otastatus s upflag 13\n");
	printf("Options:\n");
	printf("       g           gain [upflag | upmode| partstatus]\n");
	printf("       s           set [upflag | upmode| partstatus]\n");
	printf("       h           display this help text\n");
	printf("Target:\n");
	printf("       upflag      OTA update flag\n");
	printf("       upmode      OTA update mode [AB|golden]\n");
	printf("       partstatus  GPT partition status\n");
}

#define UPMODE_AB "AB"
#define UPMODE_GOLDEN "golden"

#define PARTSTATUS0 0  //000
#define PARTSTATUS1 1  //001
#define PARTSTATUS2 2  //010
#define PARTSTATUS3 3  //011
#define PARTSTATUS4 4  //100
#define PARTSTATUS5 5  //101
#define PARTSTATUS6 6  //110
#define PARTSTATUS7 7  //111

#define UPDATEFLAG_INIT 10 //init 1010
#define UPDATEFLAG_FLASH_SUCCESS 12 //flash success 1100

int main(int argc, char **argv)
{
	char flag;
	char data[16] = { 0 };
	char data2[16] = { 0 };
	char id, vid;
	int ret;
	char *para = NULL;
	char *end;
	long result;
	id = 0;
	if(argc < 3) {
		help();
		return -1;
	}

	if (veeprom_init() < 0) {
		printf("veeprom_init error\n");
		return -1;
	}

	flag = *argv[1];
	strcpy(data, argv[2]);

	switch (flag) {
	case 'g':
		if (strcmp(data, "upflag") == 0) {
			if ((ret = veeprom_read(VEEPROM_UPDATE_FLAG_OFFSET, &id, VEEPROM_UPDATE_FLAG_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			printf("%d\n", id);
			break;
		}

		if (strcmp(data, "upmode") == 0) {
			if ((ret = veeprom_read(VEEPROM_UPDATE_MODE_OFFSET, data2, VEEPROM_UPDATE_MODE_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			printf("%s\n", data2);
			break;
		}

		if (strcmp(data, "partstatus") == 0) {
			if ((ret = veeprom_read(VEEPROM_ABMODE_STATUS_OFFSET, &id, VEEPROM_ABMODE_STATUS_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			printf("%d\n", id);
			break;
		}

		if (strcmp(data, "continue_up") == 0) {
			if ((ret = veeprom_read(VEEPROM_IS_CONTINUE_UP_OFFEST, &id,
				VEEPROM_IS_CONTINUE_UP_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			printf("%d\n", id);
			break;
		}

		help();
		break;

	case 's':
		if(argc < 4) {
			help();
			break;
		}

		if (strcmp(data, "upflag") == 0) {
			para = argv[3];
			result = strtol(para, &end, 0);
			if (*end != 0) {
				printf("Error: the parameter %s is illegal !\n", para);
				help();
				return -1;
			}
			id = (char)(result & 0xff);

			if ((ret = veeprom_write(VEEPROM_UPDATE_FLAG_OFFSET, &id, VEEPROM_UPDATE_FLAG_SIZE)) < 0) {
				printf("veeprom_write ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if ((ret = veeprom_read(VEEPROM_UPDATE_FLAG_OFFSET, &vid, VEEPROM_UPDATE_FLAG_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if(id != vid) {
				printf("verify failed: %d!=%d\n", id, vid);
				veeprom_exit();
				return -1;
			}

			printf("upflag written: 0x%02x\n", id);
			break;
		}

		if (strcmp(data, "upmode") == 0) {
			memset(data, 0, sizeof(data));
			strcpy(data, argv[3]);

			if ((strcmp(data, UPMODE_AB) != 0) && (strcmp(data, UPMODE_GOLDEN) != 0)) {
				printf("Error: the parameter %s is illegal !\n", data);
				help();
				break;
			}

			if ((ret = veeprom_write(VEEPROM_UPDATE_MODE_OFFSET, data, VEEPROM_UPDATE_MODE_SIZE)) < 0) {
				printf("veeprom_write ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if ((ret = veeprom_read(VEEPROM_UPDATE_MODE_OFFSET, data2, VEEPROM_UPDATE_MODE_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if(strcmp(data2, data) != 0) {
				printf("verify failed: %s!=%s\n", data2, data);
				veeprom_exit();
				return -1;
			}

			printf("upmode written: %s\n", data2);
			break;
		}

		if (strcmp(data, "partstatus") == 0) {
			para = argv[3];
			result = strtol(para, &end, 0);
			if (*end != 0) {
				printf("Error: the parameter %s is illegal !\n", para);
				help();
				return -1;
			}
			id = (char)(result & 0xff);

			if ((ret = veeprom_write(VEEPROM_ABMODE_STATUS_OFFSET, &id,
				VEEPROM_ABMODE_STATUS_SIZE)) < 0) {
				printf("veeprom_write ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if ((ret = veeprom_read(VEEPROM_ABMODE_STATUS_OFFSET, &vid,
				VEEPROM_ABMODE_STATUS_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if(id != vid) {
				printf("verify failed: %d!=%d\n", id, vid);
				veeprom_exit();
				return -1;
			}

			printf("partstatus written: 0x%02x\n", id);
			break;
		}

		if (strcmp(data, "continue_up") == 0) {
			para = argv[3];
			result = strtol(para, &end, 0);
			if (*end != 0) {
				printf("Error: the parameter %s is illegal !\n", para);
				help();
				return -1;
			}
			id = (char)(result & 0xff);

			if ((ret = veeprom_write(VEEPROM_IS_CONTINUE_UP_OFFEST, &id,
				VEEPROM_IS_CONTINUE_UP_SIZE)) < 0) {
				printf("veeprom_write ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if ((ret = veeprom_read(VEEPROM_IS_CONTINUE_UP_OFFEST, &vid,
				VEEPROM_IS_CONTINUE_UP_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if(id != vid) {
				printf("verify failed: %d!=%d\n", id, vid);
				veeprom_exit();
				return -1;
			}

			printf("continue_up written: 0x%02x\n", id);
			break;
		}

		help();
		break;


	case 'h':
		help();
		break;

	default:
		help();
		return -1;
	};

	return 0;
}
