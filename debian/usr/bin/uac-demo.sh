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

# record mic data and send to PC
arecord -Dmicphone -c 2 -r 48000 -f S16_LE -t wav -d 500000 | aplay -Duac_playback &

# received audio data from PC and playback in speaker
arecord -Duac_recorder -c 2 -r 48000 -f S16_LE -t wav -d 500000 | aplay -Dspeaker &
