/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "veeprom.h"

static void help(void)
{
	printf("Usage:  hrut_mac [OPTIONS] <Values>\n");
	printf("Example: \n");
	printf("       hrut_mac g\n");
	printf("       hrut_mac s 00:11:22:33:44:55\n");
	printf("Options:\n");
	printf("       g   get mac value(veeprom)\n");
	printf("       s   set mac value(veeprom)\n");
	printf("       c   clear mac value\n");
	printf("       h   display this help text\n");
}

static int check_mac_para(char *c, size_t len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		if (c[i] == ':')
			continue;

		if (!isxdigit(c[i]))
			return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	char flag[6] = { 0 };
	char log[32] = { 0 };
	char *addr = NULL;
	char *p;
	int i = 0;
	int ret;
	FILE* outfile;

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
		if ((ret = veeprom_read(VEEPROM_MACADDR_OFFSET, &flag[0],
			VEEPROM_MACADDR_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		snprintf(log, sizeof(log), "%02x:%02x:%02x:%02x:%02x:%02x", flag[0], flag[1],
			flag[2], flag[3], flag[4], flag[5]);
		printf("%s\n", log);

		if(flag[0] == 0 && flag[1] == 0 && flag[2] == 0 && flag[3] == 0
			&& flag[4] == 0 && flag[5] == 0 ) {
			return -1;
		}

		outfile = fopen("/tmp/ip_mac", "wt");
		if(outfile) {
			fprintf(outfile, "%s \n", log);
			fclose(outfile);
		}
		break;
	case 's':
		if(argc < 3) {
			help();
			return -1;
		}
		addr = argv[2];

		if (check_mac_para(addr, strlen(addr))) {
			printf("Error: invalid MAC value %s !\n", addr);
			help();
			return -1;
		}

		p = strtok(addr, ":");
		while(p != NULL) {
			if(i == 6) {
				printf("Error: invalid MAC value %s !\n", addr);
				help();
				return -1;
			}

			flag[i++] = (char)strtol(p, NULL, 16);
			p = strtok(NULL, ":");
		}

		if(flag[0] == 0 && flag[1] == 0 && flag[2] == 0 && flag[3] == 0
			&& flag[4] == 0 && flag[5] == 0) {
			printf("ERR: invalid MAC, do not save MAC address from user!!\n");
			return -1;
		}

		if ((ret = veeprom_write(VEEPROM_MACADDR_OFFSET, &flag[0],
			VEEPROM_MACADDR_SIZE) < 0)) {
			printf("veeprom_write ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		break;
	case 'c':
		if ((ret = veeprom_write(VEEPROM_MACADDR_OFFSET, &flag[0],
			VEEPROM_MACADDR_SIZE) < 0)) {
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
		break;
	}
    
    veeprom_exit();
    
    return 0;
}
