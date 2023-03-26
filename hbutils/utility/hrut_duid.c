/*
 *    COPYRIGHT NOTICE
 *    Copyright 2021 Horizon Robotics, Inc.
 *    All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "veeprom.h"

#define EMMC_SERIALID_PATH 	"/sys/devices/platform/soc/a5010000.dwmmc/mmc_host/mmc0/mmc0:0001/serial"
#define SERIALID_BUF_LEN	64

int get_emmc_serial_id(void)
{
	int fd = -1;
	size_t len = 0;
	char buf[SERIALID_BUF_LEN] = {0};

	if(access(EMMC_SERIALID_PATH, F_OK) != 0) {
		return -1;
        }

        if ((fd = open(EMMC_SERIALID_PATH, O_RDONLY)) < 0) {
		return -1;
        }

	len = read(fd, buf, SERIALID_BUF_LEN);
	if (len < 0) {
		close(fd);
		return -1;
	}

	printf("%s", buf);

	close(fd);
	return 0;
}


int main(int argc, char **argv)
{
    char flag[VEEPROM_DUID_SIZE+1] = {0};
    char log[VEEPROM_DUID_SIZE+1] = {0};
    int ret;
    FILE* outfile;

    if (argc == 1) {
	get_emmc_serial_id();
	return 0;
    }

    if (veeprom_init() < 0) {
        printf("veeprom_init error\n");
        return -1;
    }
    
    switch(argv[1][0]) {
        case 'g':
            if ((ret = veeprom_read(VEEPROM_DUID_OFFSET, &flag[0], VEEPROM_DUID_SIZE) < 0)) {
                printf("veeprom_read ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }

            sprintf(log, "%s", &flag[0]);
            printf("%s", log);
            if(flag[0]==0 && flag[1]==0
                && flag[2]==0 && flag[3]==0
                && flag[4]==0 && flag[5]==0 )
            {
                printf("ERR: invalid sn, drop!!\n");
                return -1;
            }
            outfile = fopen("/tmp/duid", "wt");
            if(outfile) {
                fprintf(outfile, "%s", log);
                fclose(outfile);
            }
            break;
        case 's':
            if(argc < 3)
                return -1;
            char *pDUID = argv[2];
            int len;
            len = (int)strlen(pDUID);
            if(len>VEEPROM_DUID_SIZE)
            {
                printf("ERR: DUID(%d) was too long, MAX Length is 32! Do not save\n", len);
                return -1;
            }

            if ((ret = veeprom_write(VEEPROM_DUID_OFFSET, pDUID, VEEPROM_DUID_SIZE) < 0)) {
                printf("veeprom_write ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }
            break;
	default:
	    return -1;
    }
    
    veeprom_exit();
    
    return 0;
}
