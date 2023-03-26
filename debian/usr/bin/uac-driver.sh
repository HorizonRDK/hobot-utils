#!/bin/sh

# insmod alsa driver
modprobe ac108_driver
modprobe ac101
modprobe hobot-dma
modprobe hobot-cpudai i2s_ms=1
modprobe hobot-snd-96 snd_card=0

# launch uvc and uac2 driver
service adbd stop
#/etc/init.d/usb-gadget.sh start uvc-uac2
/etc/init.d/usb-gadget.sh start uac2
