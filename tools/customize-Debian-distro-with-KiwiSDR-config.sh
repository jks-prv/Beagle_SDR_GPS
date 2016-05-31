#!/bin/bash -e

# presumably "upD" and "scD" aliases were used to get files onto distro

echo update fresh Debian distro with KiwiSDR customizations
echo -n enter when ready:
read

echo --- apt update
apt-get update

echo --- apt clean
apt-get clean

echo --- apt install locales
apt-get -y install locales
cp Beagle_SDR_GPS/tools/locale.gen /etc
echo --- NOTE uncomment additional locales in locale.gen
locale-gen

echo --- apt upgrade
apt-get -y upgrade

echo --- set hostname
hostname kiwisdr
echo kiwisdr > /etc/hostname

echo --- install ntp
apt-get -y install ntp ntpdate
echo --- date: `date`

echo --- install git
apt-get -y install git

# this was copied with the "scD"
#echo --- clone KiwiSDR from github
#git clone https://github.com/jks-prv/Beagle_SDR_GPS.git || true
#echo -n enter when ready:
#read

echo --- install tools
apt-get -y install man
apt-get -y install make
apt-get -y install gcc
apt-get -y install g++
apt-get -y install gdb
apt-get -y install device-tree-compiler
apt-get -y install curl wget
# for killall
apt-get -y install psmisc
apt-get -y install avahi-daemon avahi-utils libnss-mdns

echo --- build KiwiSDR
echo -n enter when ready:
read
apt-get -y install libfftw3-dev libconfig-dev
(cd Beagle_SDR_GPS; make)
(cd Beagle_SDR_GPS; make install)

# Bug workaround: For Debian 8, BB-SPIDEV0 must be loaded via /boot/uEnv.txt
echo --- add load of SPIDEV0 overlay to /boot/uEnv.txt
if ! grep -q BB-SPIDEV0 /boot/uEnv.txt ; then
	echo "cape_enable=bone_capemgr.enable_partno=BB-SPIDEV0" >> /boot/uEnv.txt
fi
tail -n 8 /boot/uEnv.txt

echo --- insert micro-SD card to be written
echo -n enter when ready:
read
Beagle_SDR_GPS/tools/kiwiSDR-make-microSD-flasher-from-eMMC.sh

echo --- NOTE remove micro-SD card before reboot as it is now a flasher
echo --- script finished
