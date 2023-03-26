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
	printf("Usage:  hrut_updateflag [Options] <values>\n");
	printf("Example: \n");
	printf("       hrut_updateflag g\n");
	printf("       hrut_updateflag s 13\n");
	printf("Options:\n");
	printf("       g  get system update flag\n");
	printf("       s  set system update flag\n");
	printf("       h  display this help text\n");
}

int main(int argc, char **argv)
{
    char id, vid, flag;
    int ret;
    char *para = NULL;
	id = 0;
    if (veeprom_init() < 0) {
        printf("veeprom_init error\n");
        return -1;
    }

	if (argc < 2) {
		help();
		return -1;
	}

	flag = *argv[1];

	switch (flag) {
	case 'g':
		if ((ret = veeprom_read(VEEPROM_UPDATE_FLAG_OFFSET, &id,
				VEEPROM_UPDATE_FLAG_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		break;
	case 's':
		if (argc < 3) {
			help();
			return -1;
		}
		para = argv[2];
		if (parameter_check(para, strlen(para))) {
			printf("Error: the parameter %s is illegal !\n", para);
			help();
			return -1;
		}

		id = (char)atoi(para);
		if ((ret = veeprom_write(VEEPROM_UPDATE_FLAG_OFFSET, &id,
				VEEPROM_UPDATE_FLAG_SIZE)) < 0) {
			printf("veeprom_write ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		if ((ret = veeprom_read(VEEPROM_UPDATE_FLAG_OFFSET, &vid,
				VEEPROM_UPDATE_FLAG_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		if(id != vid) {
			printf("verify failed: %d!=%d\n", id, vid);
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
	}

    printf("%d\n", id);

    veeprom_exit();

    return id;
}
