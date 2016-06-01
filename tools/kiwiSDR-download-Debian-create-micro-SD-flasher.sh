#!/bin/bash -e
# Copyright (c) 2016 John Seamons, ZL/KF6VO

# NB: this distro image is a flasher
RCN="rcn-ee.com/rootfs/bb.org/testing/2016-05-13/console"
DISTRO="BBB-blank-debian-8.4-console-armhf-2016-05-13-2gb.img.xz"
CKSUM="c6a80fe5128d275c58ca30f427029ae699ad6f1b00b9f69254a417e2e1b479e2"

echo --- get Debian distro image from net and create micro-SD flasher
echo -n hit enter when ready: ; read

echo --- get xz-utils in case they are missing
apt-get -y install xz-utils

if test ! -f ${DISTRO} ; then
	wget https://${RCN}/${DISTRO}
else
	echo --- already seem to have the distro file, verify checksum below to be sure
fi
sha256sum ${DISTRO}
echo --- verify above checksum against:
echo ${CKSUM}
echo -n hit enter when ready: ; read

echo --- lsblk:
lsblk
echo --- insert micro-SD card
echo -n hit enter when ready: ; read

echo --- copying to micro-SD card, will take several minutes
time xzcat -v ${DISTRO} | dd of=/dev/mmcblk1

echo --- when next booted with micro-SD installed, Debian image should be copied to eMMC
echo -n hit ^C to skip reboot, else enter when ready to reboot: ; read

echo --- rebooting to re-flash eMMC from micro-SD
echo --- you should see a back-and-forth pattern in the LEDs during the copy
echo --- after all the LEDs go dark (Beagle is powered off), remove micro-SD and power up
echo --- you should now be running Debian distro
reboot
