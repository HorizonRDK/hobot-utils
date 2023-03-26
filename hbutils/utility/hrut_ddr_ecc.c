/*
 *    COPYRIGHT NOTICE
 *    Copyright 2020 Horizon Robotics, Inc.
 *    All rights reserved.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "veeprom.h"

#define MMCBLK        "/dev/mmcblk0"

int fd;
int bootmode;

static void help(void)
{
    printf("Usage:  hrut_ddr_ecc [OPTIONS] <target> <value>\n");
    printf("Example: \n");
    printf("    hrut_ddr_ecc g\n");
    printf("    hrut_ddr_ecc s on|off\n");
    printf("    hrut_ddr_ecc s option 0\n");
    printf("Options:\n");
    printf("    g   get ddr ecc config\n");
    printf("    s   set ddr ecc config\n");
    printf("    h   display this help text\n");
    printf("Target:\n");
    printf("    [ on|off ]  enable or disable ecc function, defaut off\n");
    printf("    [ option ]  ddr ecc option\n");
    printf("            0  option0: gran 0(1/8) map 127(01111111)" \
        " , default option\n");
    printf("            1  option1: gran 0(1/8) map 15(00001111)\n");
    printf("            2  option2: gran 1(1/16) map 127(01111111)\n");
    printf("            3  option3: gran 1(1/16) map 15(00001111)\n");
    printf("    [ gran ]  config ddr ecc granularity(option0)\n");
    printf("            0  1/8 of memory space\n");
    printf("            1  1/16 of memory space\n");
    printf("            2  1/32 of memory space\n");
    printf("            3  1/64 of memory space\n");
    printf("    [ map ]  config ddr ecc map (option0)\n");
    printf("            1-127, 7bit, each bit represents a protected" \
        " memory area (option0)\n");
}

static void get_mmc_ddr_ecc_config(void)
{
    unsigned short buf[8] = {0};
    unsigned int *p_board_id = NULL;
    int ecc_para = 0;
    char mbr_buf[512] = {0};
    ssize_t ret = 0;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        printf("Error: lseek sector 0 fail\n");
        return;
    }

    ret = read(fd, mbr_buf, sizeof(mbr_buf));
    if (ret < sizeof(mbr_buf)) {
        printf("Error: read sector 0 fail\n");
        return;
    }

    /* board id offset: 0xc0 */
    p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
    ecc_para = ((*p_board_id) >> 12) & 0xf;

    if ((ecc_para > 0) && (ecc_para < 5))
        printf("ECC status: on\n");
    else if (ecc_para == 0)
        printf("ECC status: off\n");
    else
        printf("Config not support!\n");


    /* query ecc granularity and map */
    if (lseek(fd, MMC_DDR_PARTITION_OFFSET * 512 + MMC_DDR_ECC_OFFSET,
        SEEK_SET) < 0) {
        printf("Error: lseek sector %d fail\n",
            MMC_DDR_PARTITION_OFFSET);
        return;
    }

    ret = read(fd, buf, sizeof(buf));
    if (ret < sizeof(buf)) {
        printf("Error: read sector %d fail\n", MMC_DDR_PARTITION_OFFSET);
        return;
    }

    printf("  ECC gran[0]: %d", buf[0]);
    printf("  ECC map[0]: %d\n", buf[4]);
    printf("  ECC gran[1]: %d", buf[1]);
    printf("  ECC map[1]: %d\n", buf[5]);
    printf("  ECC gran[2]: %d", buf[2]);
    printf("  ECC map[2]: %d\n", buf[6]);
    printf("  ECC gran[3]: %d", buf[3]);
    printf("  ECC map[3]: %d\n", buf[7]);
}

static void set_ddr_ecc_config(unsigned int ecc_para)
{
    unsigned int ddr_ecc_para_old = 0;
    unsigned int board_id_new = 0;
    char mbr_buf[512] = { 0 };
    unsigned int *p_board_id = NULL;
    int i = 0;
    int sum = 0;
    int *p_sum = NULL;
    ssize_t ret = 0;

    /* query ecc status */
    if (lseek(fd, 0, SEEK_SET) < 0) {
        printf("Error: lseek sector 0 fail\n");
        return;
    }

    ret = read(fd, mbr_buf, sizeof(mbr_buf));
    if (ret < sizeof(mbr_buf)) {
        printf("Error: read sector 0 fail\n");
        return;
    }

    /* bootinfo check sum offset: 0xc */
    p_sum = (int *)(mbr_buf + CHECK_SUM_OFFSET);
    *p_sum = 0;

    /* board id offset: 0xc0 */
    p_board_id = (unsigned int *)(mbr_buf + BOARD_ID_OFFSET);
    ddr_ecc_para_old = ((*p_board_id) >> 12) & 0xf;

    if (ddr_ecc_para_old == ecc_para) {
        printf("the ddr_alter config is same to the old\n");
        exit(0);
    }

    if (ecc_para == 0)
        board_id_new = (*p_board_id) & (~(0xf << 12));
    else
        board_id_new = ((*p_board_id) & (~(0xf << 12))) | (ecc_para << 12);

    mbr_buf[BOARD_ID_OFFSET] = (char)(board_id_new & 0xff);
    mbr_buf[BOARD_ID_OFFSET + 1] = (char)((board_id_new >> 8) & 0xff);
    mbr_buf[BOARD_ID_OFFSET + 2] = (char)((board_id_new >> 16) & 0xff);
    mbr_buf[BOARD_ID_OFFSET + 3] = (char)((board_id_new >> 24) & 0xff);

    for (i = 0; i < 275; i++)
        sum += mbr_buf[i] & 0xff;
    *p_sum = sum;

    lseek(fd, 0, SEEK_SET);
    if (write(fd, mbr_buf, sizeof(mbr_buf)) < 0) {
        printf("Error: read sector %d fail\n", MMC_DDR_PARTITION_OFFSET);
        return;
    }

    printf("change ddr_ecc_config from %d to %d\n", ddr_ecc_para_old, ecc_para);
}

static void set_ddr_ecc_granularity(unsigned short granularity)
{
    unsigned short buf[8] = {0};
    unsigned short granularity_old = 0;
    ssize_t ret = 0;

    if (lseek(fd, MMC_DDR_PARTITION_OFFSET * 512 + MMC_DDR_ECC_OFFSET,
        SEEK_SET) < 0) {
        printf("Error: fseek sector %d fail\n",
            MMC_DDR_PARTITION_OFFSET);
        return;
    }

    ret = read(fd, buf, sizeof(buf));
    if (ret < sizeof(buf)) {
        printf("Error: read sector %d fail\n", MMC_DDR_PARTITION_OFFSET);
        return;
    }

    granularity_old = buf[0];
    if (granularity == granularity_old) {
        printf("the ddr_ecc config is same to the old\n");
        exit(0);
    }
    buf[0] = granularity;

    if (lseek(fd, MMC_DDR_PARTITION_OFFSET * 512 + MMC_DDR_ECC_OFFSET,
        SEEK_SET) < 0) {
        printf("Error: fseek sector %d fail\n",
            MMC_DDR_PARTITION_OFFSET);
        return;
    }

    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        printf("write %s error\n", MMCBLK);
        exit(1);
    }

    printf("change ddr ecc granularity from %d to %d\n", granularity_old,
        granularity);
}

static void set_ddr_ecc_map(unsigned short map)
{
    unsigned short buf[8] = {0};
    unsigned short map_old = 0;
    ssize_t ret = 0;

    if (lseek(fd, MMC_DDR_PARTITION_OFFSET * 512 + MMC_DDR_ECC_OFFSET,
        SEEK_SET) < 0) {
        printf("Error: fseek sector %d fail\n",
            MMC_DDR_PARTITION_OFFSET);
    }

    ret = read(fd, buf, sizeof(buf));
    if (ret < sizeof(buf)) {
        printf("Error: read sector %d fail\n",
            MMC_DDR_PARTITION_OFFSET);
        return;
    }

    map_old = buf[4];
    if (map == map_old) {
        printf("the ddr_ecc config is same to the old\n");
        exit(0);
    }
    buf[4] = (unsigned short)map;

    if (lseek(fd, MMC_DDR_PARTITION_OFFSET * 512 + MMC_DDR_ECC_OFFSET,
        SEEK_SET) < 0) {
        printf("Error: fseek sector %d fail\n",
            MMC_DDR_PARTITION_OFFSET);
    }

    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        printf("write %s error\n", MMCBLK);
        exit(1);
    }

    printf("change ddr ecc map from %d to %d\n", map_old, map);
}


int main(int argc, char **argv)
{
    char data[32] = { 0 };
    unsigned short value = 0;
    unsigned int ecc_para = 0;
    char *para = NULL;
    int ret = 0;

	fd = open(MMCBLK, O_RDWR | O_SYNC);
    if (fd < 0) {
        printf("Open %s error!\n", MMCBLK);
        return -1;
    }

    if (argc < 2) {
        help();
        return -1;
    }

	bootmode = get_boot_mode();

    switch (argv[1][0]) {
    case 'g':
        if (argc != 2) {
            printf("hrut_ddr_ecc: parameter not support -- 'g'\n");
            help();
            break;
        }

        if (bootmode == PIN_2ND_SPINOR || bootmode == PIN_2ND_SPINAND)
            printf("error: not support set ddr_ecc in flashes\n");
        else
            get_mmc_ddr_ecc_config();
        break;
    case 's':
        if (argc < 3) {
            printf("hrut_ddr_ecc: option requires argument -- 's'\n");
            help();
            break;
        }

        /* enable/disable ecc function */
        strncpy(data, argv[2], sizeof(data));
        if (argc == 3) {
            if ((strcmp(data, "on") != 0) && (strcmp(data, "off") != 0)) {
                printf("Error: invalid argument %s !\n", argv[2]);
                break;
            }

            if (strcmp(data, "on") == 0) {
                ecc_para = 1;
            }

        	if (bootmode == PIN_2ND_SPINOR || bootmode == PIN_2ND_SPINAND) {
                printf("Error: not support set bootinfo's " \
                    "ddr_alterconfig in flashes\n");
			} else {
                set_ddr_ecc_config(ecc_para);
			}
            break;
        }

        if (argc != 4) {
            printf("hrut_ddr_ecc: parameter not support -- 's'\n");
            help();
            break;
        }

        para = argv[3];
        value = (short)atoi(argv[3]);

        ret = parameter_check(para, strlen(para));
        if (ret) {
            printf("Error: the parameter %s is illegal!\n", para);
            help();
            break;
        }

        /* enable/disable ecc function */
        if ((strcmp(data, "option") == 0)) {
            if (value > 3) {
                printf("Error: the parameter of option %d not support!\n",
						value);
                help();
                break;
            }

            if (bootmode == PIN_2ND_SPINOR || bootmode == PIN_2ND_SPINAND)
                printf("Error: not support set bootinfo's"\
						" ddr_alterconfig in flashes\n");
            else
                set_ddr_ecc_config(value + 1);
            break;
        } else if (strcmp(data, "gran") == 0) {
            if (value > 3) {
                printf("Error: the parameter of granularity %d not support!\n",
						 value);
                help();
                break;
            }

            set_ddr_ecc_granularity(value);
        } else if (strcmp(data, "map") == 0) {
            if (value < 1 || value > 127) {
                printf("Error: the parameter of map %d not support!\n", value);
                help();
                break;
            }

            set_ddr_ecc_map(value);
        } else {
            printf("Error: invalid argument %s!\n", argv[2]);
            break;
}
        break;
    case 'h':
        help();
        break;
    default:
        help();
        break;
    }

    return 0;
}
