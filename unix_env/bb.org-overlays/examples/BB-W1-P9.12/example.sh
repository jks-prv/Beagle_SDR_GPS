#!/bin/bash -e
#
# Copyright (c) 2015 Robert Nelson <robertcnelson@gmail.com>
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

if ! id | grep -q root; then
	echo "must be run as root"
	exit
fi

report_temp () {
	temp=$(cat /sys/bus/w1/devices/28-*/w1_slave | grep t= | awk -F "t=" '{print $2}')
	echo "temp: ${temp}"
}

if [ ! -d /sys/bus/w1/ ] ; then
	modprobe wire
	sleep 1
	if [ ! -d /sys/bus/w1/ ] ; then
		exit
	fi
fi

if [ ! -f /sys/bus/w1/devices/28-*/w1_slave ] ; then
	modprobe w1-gpio
	echo 'BB-W1-P9.12' > /sys/devices/platform/bone_capemgr/slots
	sleep 1
fi

if [ -f /sys/bus/w1/devices/28-*/w1_slave ] ; then
	while [ 1 ]; do report_temp; sleep 5; done
fi
