#!/bin/bash -e

SOC_sh="./SOC.sh"

broadcast () {
	if [ "x${message}" != "x" ] ; then
		echo "${message}"
		#echo "${message}" > /dev/tty0 || true
	fi
}

if ! id | grep -q root; then
	echo "must be run as root"
	exit
fi

message="-----------------------------" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast
message="----------------------------- long line long line long line long line long line long line long line long line long line long line long line long line " ; broadcast
message="KiwiSDR: test" ; broadcast
message="KiwiSDR: test" ; broadcast

if [ -f ${SOC_sh} ] ; then
	. ${SOC_sh}
fi

sleep 5
exit 42
exit 0
