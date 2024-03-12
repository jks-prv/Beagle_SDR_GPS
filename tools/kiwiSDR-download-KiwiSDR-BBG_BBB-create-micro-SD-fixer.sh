#!/bin/bash -e
# Copyright (c) 2016-2024 John Seamons, ZL4VO/KF6VO

PLAT="BBG_BBB"
CKSUM="0d1c93ff0c175e2cfae0c02e199a65709cea8441b4b1f893b77f8368df2ec006"

HOST="http://kiwisdr.com/files"
IMAGE="KiwiSDR_${PLAT}_EEPROM_fixer.img.xz"
IMAGE_FILE="/root/${IMAGE}"

echo "--- Get KiwiSDR ${PLAT} EEPROM fixer image from net and create micro-SD."
echo -n "--- Hit enter to proceed: "; read not_used

df -h .
echo "--- Make sure at least 850M is listed in the \"Avail\" column above."
echo "--- If not, type ^C and remove some files to make room."
echo -n "--- Hit ^C to stop or enter to proceed: "; read not_used

rv=$(which xzcat || true)
if test "x$rv" = "x" ; then
	echo "--- Get missing xz-utils."
    apt-get -y install debian-archive-keyring
    apt-get update
    apt-get -y install xz-utils
fi

if test ! -f ${IMAGE_FILE} ; then
	echo "--- Getting image."
	curl --output ${IMAGE_FILE} ${HOST}/${IMAGE}
else
	echo "--- Already seem to have the image file, verify checksum below to be sure."
fi
echo "--- Computing checksum..."
sha256sum ${IMAGE_FILE}
echo "--- Verify above checksum against:"
echo ${CKSUM} " Correct checksum"
echo -n "--- Hit enter when ready: "; read not_used

echo "--- Insert micro-SD card"
echo -n "--- Hit enter when ready: "; read not_used

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

echo "--- Will now copy to ${destination} which should be the micro-SD card."
echo "--- CHECK lsblk ABOVE THAT ${destination} IS THE CORRECT DEVICE BEFORE PROCEEDING!"
echo -n "--- Hit enter when ready: "; read not_used

echo "--- Copying to micro-SD card, will take about 15 minutes."
xzcat -v ${IMAGE_FILE} | dd of=${destination}
blockdev --flushbufs ${destination}
sleep 2

echo "--- IMPORTANT: How to use this EEPROM fixer SD card."
echo "--- Please read all of these instructions carefully before proceeding."
echo "--- "
echo "--- Power off the Kiwi you need to fix. Insert the sd card."
echo "--- Locate the \"boot\" button on the other side of the Beagle board from the SD card socket."
echo "--- See kiwisdr.com/boot.png for the location of the boot button/switch."
echo
echo "--- Then you MUST hold down the Beagle \"boot\" button, then apply power,"
echo "--- then release the button a few moments after all four blue LEDs light up."
echo "--- The button will give a noticeable \"click\" when it is pressed."
echo
echo "--- Now the Beagle should be running from the SD card."
echo "--- In about two minutes the Beagle will power off if the fix was successful."
echo "--- During this time the blue LED closest to the outside edge of the board will double-flash."
echo "--- If there is no power down within two minutes the fix didn't work. Try the procedure again."
echo "--- Remove the SD card after the power down."
echo "--- Power up and you should have a working Ethernet and Kiwi server."
echo
echo "--- If you can't get it to work ask for help on the Kiwi forum: forum.kiwisdr.com."
