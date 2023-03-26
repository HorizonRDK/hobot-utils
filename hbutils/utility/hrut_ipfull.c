#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "veeprom.h"

int main(int argc, char **argv)
{
    char flag[4] = {0};
    char log_ip[17];
    char log_mask[17];
    char log_gate[17];
    int ret;
    FILE* outfile;
	char *addr;
	char *p;
	int i;
    
    if(argc < 2) {
		printf("usage: \n");
		printf("hrut_ipfull g \n");
		printf("hrut_ipfull s IP(x.x.x.x) MASK(y.y.y.y) GATEWAY(z.z.z.z)\n");
        return -1;
    }
    
    if (veeprom_init() < 0) {
        printf("veeprom_init error\n");
        return -1;
    }
    
    switch(argv[1][0]) {
        case 'g':
            system("rm -f /tmp/ip_ip");
            system("rm -f /tmp/ip_mask");
            system("rm -f /tmp/ip_gw");
            ///
            /// GET IP
            ///
            if ((ret = veeprom_read(VEEPROM_IPADDR_OFFSET, &flag[0], VEEPROM_IPADDR_SIZE) < 0)) {
                printf("veeprom_read ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }

            sprintf(log_ip, "%d.%d.%d.%d",flag[0], flag[1], flag[2], flag[3]);
            printf("ip=%s\n", log_ip);

            if(flag[0]==0)
            {
                printf("ERR: invalid ip, drop IP value from eeprom!!\n");
                return -1;
            }
            outfile = fopen("/tmp/ip_ip", "wt");
            if(outfile) {
                fprintf(outfile, "%s", log_ip);
                fclose(outfile);
            }
            ///
            /// GET MASK
            ///
            if ((ret = veeprom_read(VEEPROM_IPMASK_OFFSET, &flag[0], VEEPROM_IPMASK_SIZE) < 0)) {
                printf("veeprom_read ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }

            sprintf(log_mask, "%d.%d.%d.%d",flag[0], flag[1], flag[2], flag[3]);
            printf("mask=%s\n", log_mask);
            if(flag[0]==0)
            {
                printf("ERR: invalid mask, drop MASK value from eeprom!!\n");
                return -1;
            }
            outfile = fopen("/tmp/ip_mask", "wt");
            if(outfile) {
                fprintf(outfile, "%s", log_mask);
                fclose(outfile);
            }
            ///
            /// GET GATEWAY
            ///
            if ((ret = veeprom_read(VEEPROM_IPGATE_OFFSET, &flag[0], VEEPROM_IPGATE_SIZE) < 0)) {
                printf("veeprom_read ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }

            sprintf(log_gate, "%d.%d.%d.%d",flag[0], flag[1], flag[2], flag[3]);
            printf("gw=%s\n", log_gate);
            if(flag[0]==0)
            {
                printf("ERR: invalid gateway, drop gateway value from eeprom!!\n");
                return -1;
            }
            outfile = fopen("/tmp/ip_gw", "wt");
            if(outfile) {
                fprintf(outfile, "%s", log_gate);
                fclose(outfile);
            }

            break;
        case 's':
            if(argc < 5)
            {
            	printf("hrut_ipfull s IP(x.x.x.x) MASK(y.y.y.y) GATEWAY(z.z.z.z)\n");
                return -1;
            }
            ///
            /// SET IP
            ///			
            addr = argv[2];
            i = 0;
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
            ///
            /// SET MASK
            ///
            addr = argv[3];
            i = 0;
            p = strtok(addr, ".");
            while(p!=NULL) {
                if(i==4)
                    return -1;
                flag[i++] = (char)atoi(p);
                p = strtok(NULL, "."); 
            }
            if(flag[0]==0)
            {
                printf("ERR: invalid mask, do not save MASK value from user input!!\n");
                return -1;
            }
            if ((ret = veeprom_write(VEEPROM_IPMASK_OFFSET, &flag[0], VEEPROM_IPMASK_SIZE) < 0)) {
                printf("veeprom_write ret = %d\n", ret);
                veeprom_exit();
                return -1;
            }            
            ///
            /// SET GW
            ///
            addr = argv[4];
            i = 0;
            p = strtok(addr, ".");
            while(p!=NULL) {
                if(i==4)
                    return -1;
                flag[i++] = (char)atoi(p);
                p = strtok(NULL, "."); 
            }
            if(flag[0]==0)
            {
                printf("ERR: invalid gw, do not save GATEWAY value from user input!!\n");
                return -1;
            }
            if ((ret = veeprom_write(VEEPROM_IPGATE_OFFSET, &flag[0], VEEPROM_IPGATE_SIZE) < 0)) {
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
            if ((ret = veeprom_write(VEEPROM_IPMASK_OFFSET, &flag[0], VEEPROM_IPMASK_SIZE) < 0)) {
                printf("clear ip_mask failed.\n");
            }
            if ((ret = veeprom_write(VEEPROM_IPGATE_OFFSET, &flag[0], VEEPROM_IPGATE_SIZE) < 0)) {
                printf("clear ip_gateway failed.\n");
            }
            printf("clear all ip related setting.\n");
            break;
        default:
            return -1;
    }
    
    veeprom_exit();
    
    return 0;
}
