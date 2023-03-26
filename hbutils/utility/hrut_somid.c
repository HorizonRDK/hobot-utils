/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "veeprom.h"
// SOMID(J3IC) index value
// ----------------------------
// For Quad Board:
// 1: j3a
// 2: j3b
// 3: j3c
// 4: j3d
// ---------------------------
// 0: all other j3 board with single j3 chip
//
#define SOMID_MAX 16 //!(soppose 16 is enough in practical use.)

// if multi chip exist in board.
// it is supposed using pattern x3*, j3* (*=a/b/c/d/...)
#define X2_STRING "x3"
#define J2_STRING "j3"
static int StrToNum(char *str)
{
	int chipID = 0;

	if (strstr(str, X2_STRING) != NULL ||
		strstr(str, J2_STRING) != NULL) {
		chipID = str[2] - 'a' + 1;
	} else {
		printf("uknown SOMID pattern - %s\n", str);
	}

	if (chipID < 0 || chipID > SOMID_MAX) {
		printf("invalid SOMID - %d. force it to ZERO\n", chipID);
		chipID = 0;
	}

	return chipID;
}

void PrintUsage(void)
{
	printf("hrut_somid g|s|c \n");
	printf("NOTE: hrut_somid g --eeprom   #will force get somid from eeprom\n\n");
	printf("<<<<<SOMID value meaning>>>>>\n");
	printf("0: all other xj3 board with single xj3 chip\n");
	printf("1: xj3a\n");
	printf("2: xj3b\n");
	printf("3: xj3c\n");
	printf("4: xj3d\n");
	return;
}


#define FILENAME "/proc/j3id"
int main(int argc, char **argv)
{
	char flag[4] = {0};
	char content[4];
	int ret;
	int iSomID;
	int fForceEeprom = 0;
	FILE* hFile = NULL;
	char *addr = NULL;
	size_t ret_size = 0;

	if(argc < 2) {
		PrintUsage();
		return -1;
	}

	if (veeprom_init() < 0) {
		printf("veeprom_init error\n");
		return -1;
	}

	switch(argv[1][0]) {
	case 'g':
		if (argc == 3) {
			if (strstr(argv[2], "--eeprom") != NULL) {
				fForceEeprom = 1;
			} else {
				printf("hrut_somid: parameter not support -- 'g'\n");
				PrintUsage();
				break;
			}
		} else if (argc > 3) {
			printf("hrut_somid: parameter not support -- 'g'\n");
			PrintUsage();
			break;
		}

		// !read from j2id file (1st priority)
		hFile = fopen(FILENAME, "r");
		if (hFile) {
			memset(content, 0x0, 4);
			ret_size = fread(content, 1, 3, hFile);
			if (ret_size == 0) {
				printf("read %s failed\n", FILENAME);
				fclose(hFile);
				veeprom_exit();
				return -1;
			}
			iSomID = StrToNum(content);
			fclose(hFile);
		}


		if (argc >= 3) {
			if(strstr(argv[2], "--eeprom") != NULL)
			fForceEeprom = 1;
		}

		// !read from veeprom while FILENAME not exist or  --eeprom
		if ((hFile == NULL) || fForceEeprom) {
			if ((ret = veeprom_read(VEEPROM_SOMID_OFFSET, &flag[0],
				VEEPROM_SOMID_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
		}

		iSomID = flag[0];
		if (iSomID < 0 || iSomID > SOMID_MAX) {
			printf("invalid SOMID - %d. force it to ZERO\n", iSomID);
			iSomID = 0;
			}
		}
		printf("%d\n", iSomID);
		veeprom_exit();
		return iSomID;
	case 's':
		if(argc != 3) {
			printf("hrut_somid: parameter not support -- 's'\n");
			PrintUsage();
			break;
		}

		addr = argv[2];
		ret = parameter_check(addr, strlen(addr));
		if (ret) {
			printf("Error: the SOMID %s is illegal !\n", addr);
			PrintUsage();
			return -1;
		}

		iSomID = atoi(argv[2]);
		if(iSomID < 0 || iSomID > SOMID_MAX) {
			printf("invalid SOMID - %d.\n", iSomID);
			veeprom_exit();
			return -1;
		}

		flag[0] = (char)iSomID;
		printf("set virtul som id = %d \n", iSomID);

		if ((ret = veeprom_write(VEEPROM_SOMID_OFFSET, &flag[0],
			VEEPROM_SOMID_SIZE) < 0)) {
			printf("saving board operation ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		break;
	case 'c':
		if(argc != 2) {
			printf("hrut_somid: parameter not support -- 'c'\n");
			PrintUsage();
			break;
		}

		flag[0] = 0;
		if ((ret = veeprom_write(VEEPROM_SOMID_OFFSET, &flag[0],
			VEEPROM_SOMID_SIZE) < 0)) {
			printf("clear somid operation ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		break;
	default:
		PrintUsage();
		break;
	}

	veeprom_exit();
	return 0;
}
