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
	printf("Usage:  hrut_390camtype <value>\n");
	printf("Example: \n");
	printf("       hrut_390camtype\n");
	printf("       hrut_390camtype 0\n");
	printf("       0:d3rcm, 1:d3xiamen\n");
}

int main(int argc, char **argv)
{
    char id, vid;
    int ret;

    if (veeprom_init() < 0) {
        printf("veeprom_init error\n");
        return -1;
    }

    if(argc == 1){
        if ((ret = veeprom_read(VEEPROM_CAMTYPE_OFFSET, &id, VEEPROM_CAMTYPE_SIZE) < 0)) {
            printf("veeprom_read ret = %d\n", ret);
            veeprom_exit();
            return -1;
        }
        printf("%d\n", id);
    }else if(argc == 2){
        id = (char)atoi(argv[1]);
        if ((ret = veeprom_write(VEEPROM_CAMTYPE_OFFSET, &id, VEEPROM_CAMTYPE_SIZE)) < 0) {
            printf("veeprom_write ret = %d\n", ret);
            veeprom_exit();
            return -1;
        }

        if ((ret = veeprom_read(VEEPROM_CAMTYPE_OFFSET, &vid, VEEPROM_CAMTYPE_SIZE) < 0)) {
            printf("veeprom_read ret = %d\n", ret);
            veeprom_exit();
            return -1;
        }

        if(id != vid) {
            printf("verify failed: %d!=%d\n", id, vid);
            veeprom_exit();
            return -1;
        }
    }else{
        help();    
    }

    veeprom_exit();
    
    return 0;
}
