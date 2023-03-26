/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "veeprom.h"

#define BOOT_COUNT_PATH      "/sys/class/socinfo/boot_count"

static void help(void)
{
	printf("Usage:  hrut_count [OPTIONS] [Values]\n");
	printf("Example: \n");
	printf("       hrut_count \n");
	printf("       hrut_count value \n");
	printf("Options:\n");
	printf("       h   display this help text\n");
}

static int32_t get_boot_count_from_pmu(char *count)
{
	int fd = 0;
	ssize_t ret = 0;
	char buf[8] = {0};
	fd = open(BOOT_COUNT_PATH, O_RDONLY, 0644);
	if (fd < 0) {
		printf("open %s failed:%s\n", BOOT_COUNT_PATH, strerror(errno));
		veeprom_exit();
		return -1;
	}
	ret = read(fd, buf, (int)sizeof(buf));
	if (ret < 0) {
		printf("read %s failed:%s\n", BOOT_COUNT_PATH, strerror(errno));
		close(fd);
		return -1;
	}
	*count = (char)atoi(buf);
	close(fd);
	return 0;
}

static int32_t set_boot_count_to_pmu(char *count)
{
	int fd = 0;
	ssize_t ret = 0;
	fd = open(BOOT_COUNT_PATH, O_WRONLY, 0644);
	if (fd < 0) {
		printf("open %s failed:%s\n", BOOT_COUNT_PATH, strerror(errno));
		veeprom_exit();
		return -1;
	}
	ret = write(fd, count, (int)strlen(count) + 1);
	if (ret < 0) {
		printf("write %s failed:%s\n", BOOT_COUNT_PATH, strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	char id = 0, vid = 0;
	int ret = 0;
	char para[32] = { 0 };

	if (veeprom_init() < 0) {
		printf("veeprom_init error\n");
		return -1;
	}

	veeprom_setsync(SYNC_TO_EEPROM);

	if (argc > 2) {
		help();
		veeprom_exit();
		return -1;
	}

	if (argc == 1) {
		if ((ret = veeprom_read(VEEPROM_COUNT_OFFSET, &id,
			VEEPROM_COUNT_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		if (id == 'E') {
			/* read count from pmu */
			ret = get_boot_count_from_pmu(&id);
			if (ret) {
				veeprom_exit();
				return -1;
			}
		}
		printf("%d\n", id);
	} else if (argc == 2) {
		snprintf(para, sizeof(para), "%s", argv[1]);

		if (!strcmp(para, "h")) {
			help();
			veeprom_exit();
			return 0;
		}

		if (parameter_check(para, strlen(para))) {
			printf("Error: invalid count value! \n");
			help();
			veeprom_exit();
			return -1;
		}

		id = (char)atoi(para);
		if ((ret = veeprom_read(VEEPROM_COUNT_OFFSET, &vid,
			VEEPROM_COUNT_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
		if (vid == 'E') {
			ret = set_boot_count_to_pmu(para);
			if (ret) {
				veeprom_exit();
				return -1;
			}
			ret = get_boot_count_from_pmu(&vid);
			if (ret) {
				veeprom_exit();
				return -1;
			}
		} else {
			if ((ret = veeprom_write(VEEPROM_COUNT_OFFSET, &id,
							VEEPROM_COUNT_SIZE)) < 0) {
				printf("veeprom_write ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}

			if ((ret = veeprom_read(VEEPROM_COUNT_OFFSET, &vid,
							VEEPROM_COUNT_SIZE) < 0)) {
				printf("veeprom_read ret = %d\n", ret);
				veeprom_exit();
				return -1;
			}
		}
		if (id != vid) {
			printf("verify failed: %d!=%d\n", id, vid);
			veeprom_exit();
			return -1;
		}

		printf("%d\n", id);
	}

	return id;
}
