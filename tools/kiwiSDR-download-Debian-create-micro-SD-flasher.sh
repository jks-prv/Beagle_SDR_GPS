#!/bin/bash -e
# Copyright (c) 2016-2019 John Seamons, ZL/KF6VO

# NB: untested final Debian 8 image (8.10)
# see rcn-ee.net/rootfs/bb.org/testing/keepers file
#HOST="rcn-ee.net/rootfs/bb.org/testing/2018-02-01/console"
#DISTRO="bone-debian-8.10-console-armhf-2018-02-01-1gb.img.xz"
#CKSUM="992cedbdb180b83ca42729449c8490632071dcd24a7e14b3983e6a46135de099"

# NB: this distro image is a flasher
HOST="debian.beagleboard.org/images/rcn-ee.net/rootfs/bb.org/testing/2016-11-03/console"
DISTRO="BBB-blank-debian-8.6-console-armhf-2016-11-03-2gb.img.xz"
CKSUM="9230693f2aeccbdce6dfb6fa32febe592d53a64c564e505aa90cc04bd20ac509"

echo "--- get Debian distro image from net and create micro-SD flasher"
echo -n "hit enter when ready: "; read not_used

echo "--- get xz-utils in case they are missing"
rv=$(which xzcat || true)
if test "x$rv" = "x" ; then
    apt-get -y install xz-utils
fi

if test ! -f ${DISTRO} ; then
	echo "--- getting distro"
	wget https://${HOST}/${DISTRO}
else
	echo "--- already seem to have the distro file, verify checksum below to be sure"
fi
sha256sum ${DISTRO}
echo "--- verify above checksum against:"
echo ${CKSUM}
echo -n "hit enter when ready: "; read not_used

echo "--- insert micro-SD card"
echo -n "hit enter when ready: "; read not_used

echo "--- lsblk:"
lsblk
echo "--- will now copy to mmcblk1 which should be the micro-SD card"
echo "--- CHECK lsblk ABOVE THAT THIS IS THE CORRECT DEVICE BEFORE PROCEEDING!"
echo -n "hit enter when ready: "; read not_used

echo "--- copying to micro-SD card, will take about 10 minutes"
xzcat -v ${DISTRO} | dd of=/dev/mmcblk1

echo "--- when next booted with micro-SD installed, Debian image should be copied to eMMC"
echo -n hit ^C to skip reboot, else enter when ready to reboot: ; read not_used

echo "--- rebooting to re-flash eMMC from micro-SD"
echo "--- you should see a back-and-forth pattern in the LEDs during the copy"
echo "--- after all the LEDs go dark (Beagle is powered off), remove micro-SD and power up"
echo "--- you should now be running Debian distro"
echo "--- now do 'upD' on laptop to place scripts in /root"
echo "--- run ./kiwiSDR-customize-Debian-distro-with-KiwiSDR-config.sh"
reboot
