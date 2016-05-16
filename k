#!/bin/sh
# wrapper for running the KiwiSDR app

# hack to see if we're running on a BBB
if test ! -f /etc/dogtag; then
	kiwi.bin +gps $*
	exit 0
fi

DEV=kiwi
DEVID=cape-bone-${DEV}

# select when controlling SPI hardware directly:
#DEV_SPI=${DEV}-S
#DEVID_SPI=cape-bone-${DEV_SPI}
# select when using SPIDEV driver (with built-in DMA) for SPI:
DEV_SPI=SPIDEV0
DEVID_SPI=BB-${DEV_SPI}

DEV_PRU=${DEV}-P
DEVID_PRU=cape-bone-${DEV_PRU}

CAPE=${DEVID}-00A0
SPI=${DEVID_SPI}-00A0
PRU=${DEVID_PRU}-00A0
SLOTS=`ls /sys/devices/bone_capemgr.*/slots`

# cape
if test \( ! -f /lib/firmware/${CAPE}.dtbo \) -o \( /lib/firmware/${CAPE}.dts -nt /lib/firmware/${CAPE}.dtbo \) ; then
	echo compile ${DEV} device tree;
	(cd /lib/firmware; dtc -O dtb -o ${CAPE}.dtbo -b 0 -@ ${CAPE}.dts);
# don't unload old slot because this is known to cause panics; must reboot
fi

if ! grep -q ${DEV} $SLOTS ; then
	echo load ${DEV} device tree;
	echo ${DEVID} > $SLOTS;
fi

# SPI
if test \( ! -f /lib/firmware/${SPI}.dtbo \) -o \( /lib/firmware/${SPI}.dts -nt /lib/firmware/${SPI}.dtbo \) ; then
	echo compile ${DEV_SPI} device tree;
	(cd /lib/firmware; dtc -O dtb -o ${SPI}.dtbo -b 0 -@ ${SPI}.dts);
# don't unload old slot because this is known to cause panics; must reboot
fi

if ! grep -q ${DEV_SPI} $SLOTS ; then
	echo load ${DEV_SPI} device tree;
	echo ${DEVID_SPI} > $SLOTS;
fi

# PRU (future)
if test \( ! -f /lib/firmware/${PRU}.dtbo \) -o \( /lib/firmware/${PRU}.dts -nt /lib/firmware/${PRU}.dtbo \) ; then
	echo compile ${DEV_PRU} device tree;
	(cd /lib/firmware; dtc -O dtb -o ${PRU}.dtbo -b 0 -@ ${PRU}.dts);
# don't unload old slot because this is known to cause panics; must reboot
fi

if ! grep -q ${DEV_PRU} $SLOTS ; then
	echo load ${DEV_PRU} device tree;
	echo ${DEVID_PRU} > $SLOTS;
fi

kiwi.bin +gps $*
