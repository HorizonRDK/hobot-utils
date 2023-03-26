#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "veeprom.h"

int main(int argc, char **argv)
{
    char flag[4] = {0};
    char log[17];
    int ret;
    FILE* outfile;
    
    if(argc < 2) {
        return -1;
    }
    
    if (veeprom_init() < 0) {
        printf("veeprom_init error\n");
        return -1;
    }
    
    switch(argv[1][0]) {
        case 'g':
            system("rm -f /tmp/ip_ip");
            if ((ret = veeprom_read(VEEPROM_IPADDR_OFFSET, &flag[0], VEEPROM_IPADDR_SIZE) < 0)) {
                printf("veeprom_read ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }

            sprintf(log, "%d.%d.%d.%d",flag[0], flag[1], flag[2], flag[3]);
            printf("%s\n", log);
            if(flag[0]==0)
            {
                printf("ERR: invalid ip, drop IP value from eeprom!!\n");
                return -1;
            }
            outfile = fopen("/tmp/ip_ip", "wt");
            if(outfile) {
                fprintf(outfile, "%s", log);
                fclose(outfile);
            }
            break;
        case 's':
            if(argc < 3)
                return -1;
            char *addr = argv[2];
            char *p;
            int i = 0;
            p = strtok(addr, ".");
            while(p!=NULL) {
                if(i==4)
                    return -1;
                flag[i++] = (char)atoi(p);
                p = strtok(NULL, "."); 
            }
            if(flag[0]==0)
            {
                 printf("ERR: invalid ip, do not save IP value from user input!!\n");
                 return -1;
            }
            if ((ret = veeprom_write(VEEPROM_IPADDR_OFFSET, &flag[0], VEEPROM_IPADDR_SIZE) < 0)) {
                printf("veeprom_write ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }
            break;
        case 'c':
            memset(&flag[0], 0, 4);
            if ((ret = veeprom_write(VEEPROM_IPADDR_OFFSET, &flag[0], VEEPROM_IPADDR_SIZE) < 0)) {
                printf("clear ip_ip failed.\n");
            }
            break;
        default:
            return -1;
    }
    
    veeprom_exit();
    
    return 0;
}
