#!/bin/bash -e
# Copyright (c) 2016-2022 John Seamons, ZL4VO/KF6VO

# Debian 10.3
# NB: this distro image is a flasher
OPT="--no-check-certificate"
HOST="debian.beagleboard.org/images"
DISTRO="bone-eMMC-flasher-debian-10.3-iot-armhf-2020-04-06-4gb.img.xz"
CKSUM="e339459077b83f6458cb3432494954582aedad897b9f3b62fa390dfdb010a9df"

# Debian 8.10
# NB: this distro image is a flasher
# untested final Debian 8 image (8.10)
# see rcn-ee.net/rootfs/bb.org/testing/keepers file
#OPT=
#HOST="rcn-ee.net/rootfs/bb.org/testing/2018-02-01/console"
#DISTRO="bone-debian-8.10-console-armhf-2018-02-01-1gb.img.xz"
#CKSUM="992cedbdb180b83ca42729449c8490632071dcd24a7e14b3983e6a46135de099"

# Debian 8.6
# NB: this distro image is a flasher
#OPT=
#HOST="debian.beagleboard.org/images/rcn-ee.net/rootfs/bb.org/testing/2016-11-03/console"
#DISTRO="BBB-blank-debian-8.6-console-armhf-2016-11-03-2gb.img.xz"
#CKSUM="9230693f2aeccbdce6dfb6fa32febe592d53a64c564e505aa90cc04bd20ac509"

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
	wget ${OPT} https://${HOST}/${DISTRO}
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

echo "--- copying to micro-SD card"
echo "--- will take about 20 minutes for a 4GB card, longer for higher capacity cards"
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
