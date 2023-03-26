#!/bin/sh

# insmod alsa driver
modprobe ac108_driver
modprobe ac101
modprobe hobot-dma
modprobe hobot-cpudai i2s_ms=1
modprobe hobot-snd-96 snd_card=0

# launch uvc + uac2 + rndis driver in isoc mode
service adbd stop
/etc/init.d/usb-gadget.sh start uvc-rndis-uac2 isoc

# config usb0 ip address
ifconfig usb0 192.168.100.100

# audio application
uac-gadget --only-uac-micphone &

# only usb2.0 mode (3.0 isoc need below command)
# usb_webcam -e 3 -t 10
usb_webcam -e 3 -m 2
