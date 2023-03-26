/*
 * Copyright (c) 2019 Horizon Robotics
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "veeprom.h"

void help(void)
{
	printf("usage:\n");
	printf("hrut_eeprom r ADDR SIZE : Read data in eeprom\n");
	printf("hrut_eeprom w ADDR 0xAA 0xBB ** : Write data to eeprom\n");
	printf("hrut_eeprom c : Clear eeprom \n");
	printf("hrut_eeprom d : Display all the data in eeprom\n");
}

int main(int argc, char **argv)
{
    char flag, data;
	int i;
	unsigned int addr, size;
	int ret = 0;
	char var[512] = { 0 };

    if(argc < 2) {
		help();
        return -1;
    }

	flag = *argv[1];
	switch (flag) {
		case 'r':
			addr = (unsigned int)strtoul(argv[2], 0, 16);
			if (argc < 4)
				size = 1;
			else
				size = (unsigned int)strtoul(argv[3], 0, 10);
			if (veeprom_init() < 0) {
				printf("eeprom_init error\n");
				return -1;
			}

			for (i = 0; i < size; i++) {
				if (i % 8 == 0)
					printf("0x%x:\t", addr + i);
				if (veeprom_read(addr + i, &data, 1) >= 0) {
					printf("0x%x \t", data);
				}
			}
			printf("\n");

			veeprom_exit();

			if (size == 1)
				return data;
			else
				return 0;
			break;
		case 'w':
			addr = (unsigned int)strtoul(argv[2], 0, 16);
			if (veeprom_init() < 0) {
				printf("eeprom_init error\n");
				return -1;
			}

			for (i = 3; i < argc; i++) {
				data = (char)strtoul(argv[i], 0, 16);
				var[i - 3] = data;
				printf("w 0x%x@%d\n", data, addr + i - 3);
			}
			ret = veeprom_write(addr, var, argc - 3);

			veeprom_exit();
			if (ret < 0) {
				fprintf(stderr, "veeprom write failed!\n");
				return ret;
			}
			printf("EEPROM Write Done !\n");
			break;
		case 'c':
			if (veeprom_init() < 0) {
				printf("eeprom_init error\n");
				return -1;
			}

			ret = veeprom_format();
			veeprom_exit();
			if (ret) {
				fprintf(stderr, "veeprom dump failed!\n");
				return ret;
			}
			printf("EEPROM Clear Done !\n");
			break;
		case 'd':
			if (veeprom_init() < 0) {
				printf("eeprom_init error\n");
				return -1;
			}

			ret = veeprom_dump();
			veeprom_exit();
			if (ret) {
				fprintf(stderr, "veeprom dump failed!\n");
				return ret;
			}
			break;
		default:
			help();
			return -1;
	};

	return 0;
}
