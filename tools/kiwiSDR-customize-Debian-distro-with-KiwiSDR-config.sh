#!/bin/bash -e
# Copyright (c) 2016-2019 John Seamons, ZL4VO/KF6VO

# presumably "upDF/upD" alias was used to get customization scripts (and kiwiSDR-locale.gen) onto distro

echo "update fresh Debian distro with KiwiSDR customizations"
echo -n "hit enter when ready: "; read not_used

echo "--- apt update"
apt-get -y install debian-archive-keyring
apt-get update

echo "--- apt install locales"
apt-get -y install locales
if ! grep -q BB-SPIDEV0 /boot/uEnv.txt ; then
	echo "cape_enable=bone_capemgr.enable_partno=BB-SPIDEV0" >> /boot/uEnv.txt
fi
if test ! -f kiwiSDR-locale.gen ; then
	echo "need kiwiSDR-locale.gen file"
	exit -1
fi
cp kiwiSDR-locale.gen /etc/locale.gen
echo "--- NOTE uncomment additional locales in /etc/locale.gen"
locale-gen

echo "--- apt upgrade"
apt-get -y dist-upgrade
apt-get -y autoremove

echo "--- current default time zone will be: 'Etc/UTC'"
echo "--- run 'dpkg-reconfigure tzdata' if you wish to change it"

echo "--- set hostname"
hostname kiwisdr
echo kiwisdr > /etc/hostname

echo "--- install ntp"
apt-get -y install ntp ntpdate
echo "--- date:" `date`

echo "--- install git"
apt-get -y install git

# Beagle_SDR_GPS sources need to be a git clone so autoupdate scheme works
echo "--- clone KiwiSDR from github"
echo -n "hit enter when ready: "; read not_used
git clone https://github.com/jks-prv/Beagle_SDR_GPS.git || true

# in addition to what Kiwi "make install" will get
echo "--- install tools"
apt-get -y install man
apt-get -y install make

# still install gcc etc even though Kiwi now mostly uses clang
apt-get -y install gcc
apt-get -y install g++
apt-get -y install gdb

echo "--- build KiwiSDR"
echo -n "hit enter when ready: "; read not_used
(cd Beagle_SDR_GPS; make)
(cd Beagle_SDR_GPS; make install)

# Starting with Debian 8.4 BB-SPIDEV0 must be loaded via /boot/uEnv.txt
echo "--- add load of SPIDEV0 overlay to /boot/uEnv.txt"
if ! grep -q BB-SPIDEV0 /boot/uEnv.txt ; then
	echo "cape_enable=bone_capemgr.enable_partno=BB-SPIDEV0" >> /boot/uEnv.txt
fi
tail -n 8 /boot/uEnv.txt

echo "--- remove customization scripts"
rm -f kiwiSDR*.sh kiwiSDR*.gen

echo "--- some upgraded packages require reboot to finish installation"
echo "--- eMMC image will not be ready for copy to micro-SD before then"
echo "--- after reboot insert micro-SD card to be written"
echo "--- run ./Beagle_SDR_GPS/tools/kiwiSDR-make-microSD-flasher-from-eMMC.sh"
echo "--- then remove micro-SD card as it is now a flasher"
echo -n "hit ^C to skip reboot, else enter when ready to reboot: "; read not_used
reboot
