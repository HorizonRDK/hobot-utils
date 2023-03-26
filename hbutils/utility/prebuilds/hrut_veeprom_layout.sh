#!/bin/sh

function bar()
{
    yes "-" | sed $1'q' | tr -d '\n'
}
function menu()
{
    if [ $4 -le $length_old ]; then
        len=$length_old
    else
        len=$4
    fi
    printf "%16s %6s %4s\t$(bar $(($len+2)))\n"
    printf "%16s %6s %4s\t|%s|$6\n" "$1" "$2" "$3" "$5"
    length_old=$4
}

length_old=0

boot_mode=$(cat /sys/class/socinfo/boot_mode)
# NOR Boot
if [ "$boot_mode" = "5" ];then
    dev="/dev/mtd0"
elif [ "$boot_mode" = "1" ];then
    dev="/dev/ubi10_0"
elif [ "$boot_mode" = "0" ];then
    dev="/dev/mmcblk0p1"
else
    echo "unknown boot_mode"
    exit 1
fi

printf "%16s %6s %4s\tValue\n" " " "Offset" "Size"

boardid=$(hexdump -n 4 -s 132 -e '4/1 "%02x"' $dev)
length=${#boardid}
menu "board id" 132 4 $length "$boardid"

macaddr=$(hexdump -n 6 -s 2 -e '1/1 "%02x" ":"' $dev)
macaddr=${macaddr:0:-1}
length=${#macaddr}
menu "mac addr" 2 6 $length "$macaddr"

updateflag=$(hexdump -n 1 -s 8 -e '1/1 "0x%x"' $dev)
length=${#updateflag}
menu "update flag" 8 1 $length $updateflag \
    "\tupdate_success[$((($updateflag>>3)&0x1))],\
write_storage[$((($updateflag>>2)&0x1))],\
first_try[$((($updateflag>>1)&0x1))],\
app_success[$((($updateflag)&0x1))]"

resetreason=$(hexdump -n 8 -s 9 -e '/1 "%c"' $dev)
length=${#resetreason}
menu "reset reason" 9 8 $length "$resetreason"

ipaddr=$(hexdump -n 4 -s 17 -e '1/1 "%d" ":"' $dev)
length=${#ipaddr}
menu "ip addr" 17 4 $length "$ipaddr"

ipmask=$(hexdump -n 4 -s 21 -e '1/1 "%d" ":"' $dev)
length=${#ipmask}
menu "ip mask" 21 4 $length "$ipmask"

ipgate=$(hexdump -n 4 -s 25 -e '1/1 "%d" ":"' $dev)
length=${#ipgate}
menu "ip gate" 25 4 $length "$ipgate"

update_mode=$(hexdump -n 8 -s 29 -e '/1 "%c"' $dev)
length=${#update_mode}
menu "update mode" 29 8 $length "$update_mode"

ab_mode=$(hexdump -n 1 -s 37 -e '/1 "%x"' $dev)
length=${#ab_mode}
menu "AB mode" 37 1 $length "$ab_mode"

count=$(hexdump -n 1 -s 38 -e '/1 "%d"' $dev)
length=${#count}
menu "fail boot count" 38 1 $length "$count"

somid=$(hexdump -n 1 -s 39 -e '/1 "%d"' $dev)
length=${#somid}
menu "quad som id" 39 1 $length "$somid"

peri_pll=$(hexdump -n 16 -s 40 -e '/1 "%c"' $dev)
length=${#peri_pll}
menu "peri pll" 40 16 $length "$peri_pll"

# empty from 56 to 220

duid=$(hexdump -n 32 -s 220 -e '/1 "%02x"' $dev)
length=${#duid}
menu "duid" 220 32 $length "$duid"

printf "%16s %6s %4s\t$(bar $(($length+2)))\n"
