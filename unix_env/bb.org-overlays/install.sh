#!/bin/bash -e
#
# Copyright (c) 2015-2017 Robert Nelson <robertcnelson@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

arch=$(uname -m)

if [ "x${arch}" != "xarmv7l" ] ; then
	echo ""
	echo "Error: this script: [$0] is not supported to run under [${arch}]"
	echo "-----------------------------"
	exit
fi

check_dpkg () {
	LC_ALL=C dpkg --list | awk '{print $2}' | grep "^${pkg}" >/dev/null || deb_pkgs="${deb_pkgs}${pkg} "
}

unset deb_pkgs
pkg="build-essential"
check_dpkg

if [ ! -f /usr/share/initramfs-tools/hooks/dtbo ] ; then
	sudo cp -v ./tools/dtbo /usr/share/initramfs-tools/hooks/dtbo
	sudo chmod +x /usr/share/initramfs-tools/hooks/dtbo
else
	if [ ! -x /usr/share/initramfs-tools/hooks/dtbo ] ; then
		sudo chmod +x /usr/share/initramfs-tools/hooks/dtbo
	fi
fi

if [ "${deb_pkgs}" ] ; then
	echo "Installing: ${deb_pkgs}"
	sudo apt-get update
	sudo apt-get -y install ${deb_pkgs}
	sudo apt-get clean
fi

update_initramfs () {
	if [ -f /boot/initrd.img-$(uname -r) ] ; then
		sudo update-initramfs -u -k $(uname -r)
	else
		sudo update-initramfs -c -k $(uname -r)
	fi
}

make clean
make
sudo make install
update_initramfs
sudo cp -v ./tools/beaglebone-universal-io/config-pin /usr/local/bin/
echo "cape overlays have been built and added to /lib/firmware & /boot/initrd.img-`uname -r`, please reboot"
#
