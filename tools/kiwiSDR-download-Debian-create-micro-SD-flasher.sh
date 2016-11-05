#!/bin/bash -e
# Copyright (c) 2016 John Seamons, ZL/KF6VO

# NB: this distro image is a flasher
RCN="rcn-ee.com/rootfs/bb.org/testing/2016-11-03/console"
DISTRO="BBB-blank-debian-8.6-console-armhf-2016-11-03-2gb.img.xz"
CKSUM="9230693f2aeccbdce6dfb6fa32febe592d53a64c564e505aa90cc04bd20ac509"

# no longer on site
#RCN="rcn-ee.com/rootfs/bb.org/testing/2016-06-19/console"
#DISTRO="BBB-blank-debian-8.5-console-armhf-2016-06-19-2gb.img.xz"
#CKSUM="c68fd66873a5922d8746879f78eba6989be2e05103fb0979f9a95ba63545e957"

#RCN="rcn-ee.com/rootfs/bb.org/testing/2016-05-13/console"
#DISTRO="BBB-blank-debian-8.4-console-armhf-2016-05-13-2gb.img.xz"
#CKSUM="c6a80fe5128d275c58ca30f427029ae699ad6f1b00b9f69254a417e2e1b479e2"

echo "--- get Debian distro image from net and create micro-SD flasher"
echo -n hit enter when ready: ; read

echo "--- get xz-utils in case they are missing"
apt-get -y install xz-utils

if test ! -f ${DISTRO} ; then
	echo "--- getting distro"
	wget https://${RCN}/${DISTRO}
else
	echo "--- already seem to have the distro file, verify checksum below to be sure"
fi
sha256sum ${DISTRO}
echo "--- verify above checksum against:"
echo ${CKSUM}
echo -n hit enter when ready: ; read

echo "--- insert micro-SD card"
echo -n hit enter when ready: ; read

echo "--- lsblk:"
lsblk
echo -n hit enter when ready: ; read

echo "--- copying to micro-SD card, will take about 10 minutes"
time xzcat -v ${DISTRO} | dd of=/dev/mmcblk1

echo "--- when next booted with micro-SD installed, Debian image should be copied to eMMC"
echo -n hit ^C to skip reboot, else enter when ready to reboot: ; read

echo "--- rebooting to re-flash eMMC from micro-SD"
echo "--- you should see a back-and-forth pattern in the LEDs during the copy"
echo "--- after all the LEDs go dark (Beagle is powered off), remove micro-SD and power up"
echo "--- you should now be running Debian distro"
echo "--- now do 'upD' on laptop to place scripts in /root"
echo "--- run ./kiwiSDR-customize-Debian-distro-with-KiwiSDR-config.sh"
reboot
