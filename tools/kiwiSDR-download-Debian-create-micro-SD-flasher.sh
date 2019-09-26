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

# NB: this distro image is a flasher
#HOST="debian.beagleboard.org/images/rcn-ee.net/rootfs/bb.org/testing/2016-06-19/console"
#DISTRO="BBB-blank-debian-8.5-console-armhf-2016-06-19-2gb.img.xz"
#CKSUM="c68fd66873a5922d8746879f78eba6989be2e05103fb0979f9a95ba63545e957"

echo "--- get Debian distro image from net and create micro-SD flasher"
echo -n "--- hit enter when ready: "; read not_used

rv=$(which xzcat || true)
if test "x$rv" = "x" ; then
	echo "--- get missing xz-utils"
    apt-get -y install debian-archive-keyring
    apt-get update
    apt-get -y install xz-utils
fi

if test ! -f ${DISTRO} ; then
	echo "--- getting distro"
	wget https://${HOST}/${DISTRO}
else
	echo "--- already seem to have the distro file, verify checksum below to be sure"
fi
echo "--- computing checksum..."
sha256sum ${DISTRO}
echo "--- verify above checksum against:"
echo ${CKSUM} " correct checksum"
echo -n "--- hit enter when ready: "; read not_used

echo "--- insert micro-SD card"
echo -n "--- hit enter when ready: "; read not_used

echo "--- lsblk:"
lsblk

root_drive="$(cat /proc/cmdline | sed 's/ /\n/g' | grep root=UUID= | awk -F 'root=' '{print $2}' || true)"
if [ ! "x${root_drive}" = "x" ] ; then
	root_drive="$(/sbin/findfs ${root_drive} || true)"
else
	root_drive="$(cat /proc/cmdline | sed 's/ /\n/g' | grep root= | awk -F 'root=' '{print $2}' || true)"
fi
boot_drive="${root_drive%?}1"

if [ "x${boot_drive}" = "x/dev/mmcblk0p1" ] ; then
	source="/dev/mmcblk0"
	destination="/dev/mmcblk1"
fi

if [ "x${boot_drive}" = "x/dev/mmcblk1p1" ] ; then
	source="/dev/mmcblk1"
	destination="/dev/mmcblk0"
fi

echo "--- will now copy to ${destination} which should be the micro-SD card"
echo "--- CHECK lsblk ABOVE THAT ${destination} IS THE CORRECT DEVICE BEFORE PROCEEDING!"
echo -n "--- hit enter when ready: "; read not_used

echo "--- copying to micro-SD card, will take about 10 minutes"
xzcat -v ${DISTRO} | dd of=${destination}

echo "--- when next booted with micro-SD installed, KiwiSDR image should be copied to Beagle eMMC flash"
echo -n "--- hit ^C to skip reboot, else enter when ready to reboot: "; read not_used

echo "--- rebooting with flasher micro-SD installed will COMPLETELY OVERWRITE THIS BEAGLE's FILESYSTEM!"
echo -n "--- ARE YOU SURE? enter when ready to reboot: "; read not_used

echo "--- okay, rebooting to re-flash Beagle eMMC flash from micro-SD"
echo "--- you should see a back-and-forth pattern in the LEDs during the copy"
echo "--- after all the LEDs go dark (Beagle is powered off), remove micro-SD and power up"
echo "--- you should now be running Debian distro"
echo "--- then do 'upDF/upD' on laptop to place scripts in /root"
echo "--- run ./kiwiSDR-customize-Debian-distro-with-KiwiSDR-config.sh"
echo "--- rebooting now..."
reboot
