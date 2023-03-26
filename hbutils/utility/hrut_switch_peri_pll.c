#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "veeprom.h"


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
		if ((ret = veeprom_write(VEEPROM_PERI_PLL_OFFSET, vid, VEEPROM_PERI_PLL_SIZE)) < 0) {
			printf("veeprom_write ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}

		if ((ret = veeprom_read(VEEPROM_PERI_PLL_OFFSET, id, VEEPROM_PERI_PLL_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
	} else {
		memset(id, 0, sizeof(id));
		if ((ret = veeprom_read(VEEPROM_PERI_PLL_OFFSET, id, VEEPROM_PERI_PLL_SIZE) < 0)) {
			printf("veeprom_read ret = %d\n", ret);
			veeprom_exit();
			return -1;
		}
	}
    printf("%s\n", id);

    veeprom_exit();

    return 0;
}
