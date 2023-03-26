/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "veeprom.h"

#define UPMODE_AB "AB"
static void help(void)
{
	printf("Usage:  hrut_resetreason [Values]\n");
	printf("Example: \n");
	printf("       hrut_resetreason\n");
	printf("       hrut_resetreason recovery\n");
	printf("Options:\n");
	printf("       h   display this help text\n");
}

int main(int argc, char **argv)
{
    char id[64] = { 0 }, vid[64] = { 0 };
    int ret;

    if (veeprom_init() < 0) {
        printf("veeprom_init error\n");
        return -1;
    }

    veeprom_setsync(SYNC_TO_EEPROM);

    if(argc == 2) {
		strcpy(vid, argv[1]);

		if (!strcmp(vid, "h")) {
			help();
			veeprom_exit();
			return 0;
		}

		if ((strcmp(vid, "normal") !=0) && ((strcmp(vid, "recovery") !=0))
			&& (strcmp(vid, "uboot") !=0) && (strcmp(vid, "boot") !=0)
			&& (strcmp(vid, "system") !=0) && (strcmp(vid, "all") !=0)) {
				printf("Error: the parameter %s is illegal !\n", vid);
				return -1;
		}

		if (strcmp(vid, "recovery") == 0) {
			if ((ret = veeprom_read(VEEPROM_UPDATE_MODE_OFFSET, id,
				VEEPROM_UPDATE_MODE_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}
			if (strcmp(id, UPMODE_AB) == 0) {
				printf("Error: AB model not support recovery\n");
				return -1;
			}
		}

		if ((ret = veeprom_write(VEEPROM_RESET_REASON_OFFSET, vid,
				VEEPROM_RESET_REASON_SIZE)) < 0) {
			printf("veeprom_write ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		memset(id, 0, sizeof(id));
		if ((ret = veeprom_read(VEEPROM_RESET_REASON_OFFSET, id,
				VEEPROM_RESET_REASON_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
	} else {
		memset(id, 0, sizeof(id));
		if ((ret = veeprom_read(VEEPROM_RESET_REASON_OFFSET, id,
				VEEPROM_RESET_REASON_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
	}
    printf("%s\n", id);

    veeprom_exit();

    return 0;
}
