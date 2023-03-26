=====================================================================================================================================
hrut_updateflag
 - set update status flag in eeprom

before update, set updating flag:
hrut_updateflag 1

after update finish, clear updating flag:
hrut_updateflag 0

if updating flag was detected in u-boot, system will entry the "recovery mode": 
boot with the zImage, dtb, FPGA RBF file from "golden" folder in vfat partition

=====================================================================================================================================
hrut_fpgaconf
 - choose the FPGA RBF config file
ex:
    hrut_fpgaconf 0
        - choose the fpgaconf.ini in vfat partition
        
    hrut_fpgaconf 1
        - choose the fpgaconf1.ini in vfat partition
        
        
example 1 of contents in fpgaconf.ini:
ghrd_10as066n2
 - that means load the ghrd_10as066n2.core.rbf, ghrd_10as066n2.periph.rbf or ghrd_10as066n2.rbf depends on the uboot DTS setting.

example 2 of contents in fpgaconf.ini:
ces2018
 - that means load the ces2018.core.rbf, ces2018.periph.rbf or ces2018.rbf depends on the uboot DTS setting.
 
=====================================================================================================================================
hrut_mac
 - get/set board MAC address
 - $? == 0 success
ex:
    hrut_mac g
        - get mac, if success, the MAC address will be write to stdout and /tmp/mac, show as "00:58:91:d9:3f:8e"
    hrut_mac s 00:58:91:d9:3f:8e
        - set mac
        
=====================================================================================================================================
hrut_ip
 - get/set board IP address
 - $? == 0 success
ex:
    hrut_ip g
        - get IP, if success, the IP address will be write to stdout and /tmp/IP, show as "192.168.1.10"
    hrut_ip s 192.168.1.10
        - set IP
        
=====================================================================================================================================
hrut_boardconf
 - board no setting
ex:
    hrut_boardconf
        - get current
    hrut_boardconf 1
        - set board no to "1"
=====================================================================================================================================
hrut_bpuprofile
 - bpu profiler tools 
parameter:
 -h help information
 -b which bpu core 0-bpu0,1-bpu1,2-all
 -p power,0-power off, 1-power on
 -c clock 0-clock off, 1-clock on
 -t time,no argument
 -f bpu frequency,argument:n
 -r get bpu ratio,n-n times,0-always get 
ex:
    hbrt_bpuprofile -b 2 -r 0 
	- get bpu0/1 using ratio forever
=====================================================================================================================================
