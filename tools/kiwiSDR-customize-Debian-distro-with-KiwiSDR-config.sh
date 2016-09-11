#!/bin/bash -e
# Copyright (c) 2016 John Seamons, ZL/KF6VO

# presumably "upD" alias was used to get customization files onto distro

echo update fresh Debian distro with KiwiSDR customizations
echo -n hit enter when ready: ; read

echo "--- apt update"
apt-get update

echo "--- apt install locales"
apt-get -y install locales
cp kiwiSDR-locale.gen /etc/locale.gen
echo "--- NOTE uncomment additional locales in /etc/locale.gen"
locale-gen

echo "--- apt upgrade"
apt-get -y upgrade

echo "--- set hostname"
hostname kiwisdr
echo kiwisdr > /etc/hostname

echo "--- install ntp"
apt-get -y install ntp ntpdate
echo "--- date:" `date`

echo "--- install git"
apt-get -y install git

# Beagle_SDR_GPS sources need to be a git clone so update scheme works
# This is hardcoded to jks's repo until a better plan for how forks/branches
# _should_ interact as part of a managed install.
echo "--- clone KiwiSDR from github"
echo -n hit enter when ready: ; read
repo="https://github.com/jks-prv/Beagle_SDR_GPS"
git clone $repo || true

echo "--- install tools"
apt-get -y install man
apt-get -y install make
apt-get -y install gcc
apt-get -y install g++
apt-get -y install gdb
apt-get -y install device-tree-compiler
apt-get -y install curl wget
apt-get -y install xz-utils
# for killall
apt-get -y install psmisc
apt-get -y install avahi-daemon avahi-utils libnss-mdns

echo "--- build KiwiSDR"
echo -n hit enter when ready: ; read
apt-get -y install libfftw3-dev libconfig-dev
(cd Beagle_SDR_GPS; make)
(cd Beagle_SDR_GPS; make install)

# Bug workaround: For Debian 8.4, BB-SPIDEV0 must be loaded via /boot/uEnv.txt
echo "--- add load of SPIDEV0 overlay to /boot/uEnv.txt"
if ! grep -q BB-SPIDEV0 /boot/uEnv.txt ; then
	echo "cape_enable=bone_capemgr.enable_partno=BB-SPIDEV0" >> /boot/uEnv.txt
fi
tail -n 8 /boot/uEnv.txt

echo "--- remove customization files"
rm -f kiwiSDR*.sh kiwiSDR*.gen

echo "--- some upgraded packages require reboot to finish installation"
echo "--- eMMC image will not be ready for copy to micro-SD before then"
echo "--- after reboot insert micro-SD card to be written"
echo "--- run ./Beagle_SDR_GPS/tools/kiwiSDR-make-microSD-flasher-from-eMMC.sh"
echo "--- then remove micro-SD card as it is now a flasher"
echo -n hit ^C to skip reboot, else enter when ready to reboot: ; read
reboot
