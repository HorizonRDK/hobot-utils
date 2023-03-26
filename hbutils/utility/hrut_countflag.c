#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "veeprom.h"

void help(void)
{
	printf("Usage:  hrut_countflag [Options] <Values>\n");
	printf("Example: \n");
	printf("       hrut_countflag s on\n");
	printf("       hrut_countflag g\n");
	printf("Options:\n");
	printf("       s config OTA count flag: on or off\n");
	printf("       g get the status of OTA count flag\n");
	printf("       h display this help text\n");
	printf("Values:\n");
	printf("       on enable OTA count function\n");
	printf("       off disable OTA count function\n");
}

int main(int argc, char **argv)
{
	char id = 0, flag = 0;
	int ret;
	char data[16] = { 0 };

	if (veeprom_init() < 0) {
		printf("veeprom_init error\n");
		return -1;
	}

	if (argc < 2) {
		help();
		return -1;
	}
	flag = *argv[1];

	veeprom_setsync(SYNC_TO_EEPROM);

	switch (flag) {
	case 'g':
		if(argc != 2) {
			help();
			break;
		}

		if ((ret = veeprom_read(VEEPROM_COUNT_FLAG_OFFSET, &id,
			VEEPROM_COUNT_FLAG_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		if (id == 1)
			printf("on\n");
		else
			printf("off\n");
		break;

	case 's':
		if(argc != 3) {
			help();
			break;
		}

		strcpy(data, argv[2]);
		if (strcmp(data, "on") == 0) {
			id = 1;
		} else if (strcmp(data, "off") == 0) {
			id = 0;
		} else {
			help();
			break;
		}

		if ((ret = veeprom_write(VEEPROM_COUNT_FLAG_OFFSET, &id,
			VEEPROM_COUNT_FLAG_SIZE)) < 0) {
			printf("veeprom_write ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		if (id == 1)
			printf("on\n");
		else
			printf("off\n");
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
