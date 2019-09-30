#!/bin/sh -e
# /opt/scripts/tools/version.sh

if ! id | grep -q root; then
	echo "must be run as root"
	exit
fi

omap_bootloader () {
	unset test_var
	test_var=$(dd if=${drive} count=6 skip=393248 bs=1 2>/dev/null || true)
	if [ "x${test_var}" = "xU-Boot" ] ; then
		uboot=$(dd if=${drive} count=32 skip=393248 bs=1 2>/dev/null || true)
		uboot=$(echo ${uboot} | awk '{print $2}')
		echo "bootloader:[${label}]:[${drive}]:[U-Boot ${uboot}]:[location: dd MBR]"
	else
		if [ -f /boot/uboot/u-boot.img ] ; then
			if [ -f /usr/bin/mkimage ] ; then
				unset uboot
				uboot=$(/usr/bin/mkimage -l /boot/uboot/u-boot.img | grep Description | head -n1 | awk '{print $3}' 2>/dev/null || true)
				if [ ! "x${uboot}" = "x" ] ; then
					echo "bootloader:[${label}]:[${drive}]:[U-Boot ${uboot}]:[location: fatfs /boot/uboot/MLO]"
				else
					unset uboot
					uboot=$(/usr/bin/mkimage -l /boot/uboot/u-boot.img | grep Name:| head -n1 | awk '{print $4}' 2>/dev/null || true)
					if [ ! "x${uboot}" = "x" ] ; then
						echo "bootloader:[${label}]:[${drive}]:[U-Boot ${uboot}]:[location: fatfs /boot/uboot/MLO]"
					fi
				fi
			fi
		else
			if [ -b ${drive}p1 ] ; then
				#U-Boot SPL 2014.04
				unset test_var
				test_var=$(dd if=${drive}p1 count=6 skip=284077 bs=1 2>/dev/null || true)
				if [ "x${test_var}" = "xU-Boot" ] ; then
					uboot=$(dd if=${drive}p1 count=34 skip=284077 bs=1 2>/dev/null || true)
					uboot=$(echo ${uboot} | awk '{print $3}')
					echo "bootloader:[${label}]:[${drive}]:[U-Boot ${uboot}]:[location: fatfs /boot/uboot/MLO]"
				fi
			fi
		fi
	fi
}

dpkg_check_version () {
	unset pkg_version
	pkg_version=$(dpkg -l | awk '$2=="'$pkg'" { print $3 }' || true)
	if [ ! "x${pkg_version}" = "x" ] ; then
		echo "pkg:[$pkg]:[$pkg_version]"
	else
		echo "WARNING:pkg:[$pkg]:[NOT_INSTALLED]"
	fi
}

dpkg_check_version_replaced () {
	unset pkg_version
	pkg_version=$(dpkg -l | awk '$2=="'$pkg'" { print $3 }' || true)
	if [ ! "x${pkg_version}" = "x" ] ; then
		echo "pkg:[$pkg]:[$pkg_version]:[GOT_REPLACED_BY_NEXT]"
	fi
}

if [ -f /usr/bin/git ] ; then
	if [ -d /opt/scripts/ ] ; then
		old_dir="`pwd`"
		cd /opt/scripts/ || true
		echo "git:/opt/scripts/:[`/usr/bin/git rev-parse HEAD`]"
		cd "${old_dir}" || true
	fi
fi

if [ -f /sys/bus/i2c/devices/0-0050/eeprom ] ; then
	board_eeprom=$(hexdump -e '8/1 "%c"' /sys/bus/i2c/devices/0-0050/eeprom -n 28 | cut -b 5-28 || true)
	echo "eeprom:[${board_eeprom}]"
fi

device_model=$(cat /proc/device-tree/model | sed "s/ /_/g" | tr -d '\000')

case "${device_model}" in
SanCloud_BeagleBone_Enhanced)
	wifi_8723bu_driver=$(lsmod | grep 8723bu | grep -v cfg | awk '{print $1}' || true)
	if [ "x${wifi_8723bu_driver}" != "x" ] ; then
		echo "model:[${device_model}]:WiFi AP Enabled:[https://github.com/lwfinger/rtl8723bu]"
	else
		echo "model:[${device_model}]:WiFi AP Broken on Mainline"
	fi
	;;
*)
	echo "model:[${device_model}]"
	;;
esac

if [ -f /etc/dogtag ] ; then
	echo "dogtag:[`cat /etc/dogtag`]"
fi

#if [ -f /bin/lsblk ] ; then
#	lsblk | sed 's/^/partition_table:[/' | sed 's/$/]/'
#fi

machine=$(cat /proc/device-tree/model | sed "s/ /_/g" | tr -d '\000')

if [ "x${SOC}" = "x" ] ; then
	case "${machine}" in
	TI_AM5728_Beagle*)
		mmc0_label="microSD-(primary)"
		mmc1_label="eMMC-(secondary)"
		;;
	TI_OMAP5_uEVM_board)
		mmc0_label="eMMC-(secondary)"
		mmc1_label="microSD-(primary)"
		;;
	TI_AM335x_PocketBeagle)
		mmc0_label="microSD"
		;;
	*)
		mmc0_label="microSD-(push-button)"
		mmc1_label="eMMC-(default)"
		;;
	esac
fi

if [ -b /dev/mmcblk0 ] ; then
	label=${mmc0_label}
	drive=/dev/mmcblk0
	omap_bootloader
fi

if [ -b /dev/mmcblk1 ] ; then
	label=${mmc1_label}
	drive=/dev/mmcblk1
	omap_bootloader
fi

if [ -f /proc/device-tree/chosen/base_dtb ] ; then
	echo "UBOOT: Booted Device-Tree:[`cat /proc/device-tree/chosen/base_dtb`]"
	if [ -d /proc/device-tree/chosen/overlays/ ] ; then
		ls /proc/device-tree/chosen/overlays/ -p | grep -v / | grep -v name | sed 's/^/UBOOT: Loaded Overlay:[/' | sed 's/$/]/'
	fi
fi

echo "kernel:[`uname -r`]"

if [ -f /usr/bin/nodejs ] ; then
	echo "nodejs:[`/usr/bin/nodejs --version`]"
fi

if [ -f /boot/uEnv.txt ] ; then
	unset test_var
	test_var=$(cat /boot/uEnv.txt | grep -v '#' | grep dtb | grep -v dtbo || true)
	if [ "x${test_var}" != "x" ] ; then
		echo "device-tree-override:[$test_var]"
	fi
fi

if [ -f /boot/uEnv.txt ] ; then
	echo "/boot/uEnv.txt Settings:"
	unset test_var
	test_var=$(cat /boot/uEnv.txt | grep -v '#' | grep enable_uboot_overlays=1 || true)
	if [ "x${test_var}" != "x" ] ; then
		if [ -f /uEnv.txt ] ; then
			echo "/uEnv.txt exists, uboot overlays is DISABLED, remove /uEnv.txt"
		fi
		cat /boot/uEnv.txt | grep uboot_ | grep -v '#' | sed 's/^/uboot_overlay_options:[/' | sed 's/$/]/'
		cat /boot/uEnv.txt | grep dtb_overlay | grep -v '#' | sed 's/^/uboot_overlay_options:[/' | sed 's/$/]/'
	fi
fi

echo "pkg check: to individually upgrade run: [sudo apt install --only-upgrade <pkg>]"
pkg="bb-cape-overlays" ; dpkg_check_version
pkg="bb-wl18xx-firmware" ; dpkg_check_version
pkg="kmod" ; dpkg_check_version
pkg="roboticscape" ; dpkg_check_version_replaced
pkg="librobotcontrol" ; dpkg_check_version

if [ -d /home/debian/ ] ; then
	pkg="firmware-ti-connectivity" ; dpkg_check_version
	echo "groups:[`groups debian`]"
fi

if [ -d /home/machinekit/ ] ; then
	pkg="firmware-ti-connectivity" ; dpkg_check_version
	echo "groups:[`groups machinekit`]"
fi

if [ -d /home/ubuntu/ ] ; then
	echo "groups:[`groups ubuntu`]"
fi

if [ -d /home/beagle/ ] ; then
	echo "groups:[`groups beagle`]"
fi

echo "cmdline:[`cat /proc/cmdline`]"

echo "dmesg | grep remote"
dmesg | grep remote || true
echo "dmesg | grep pru"
dmesg | grep pru || true
echo "dmesg | grep pinctrl-single"
dmesg | grep pinctrl-single || true
echo "dmesg | grep gpio-of-helper"
dmesg | grep gpio-of-helper || true
echo "lsusb"
lsusb || true
echo "END"

#
