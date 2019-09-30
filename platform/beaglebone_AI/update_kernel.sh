#!/bin/sh -e
#
# Copyright (c) 2014-2019 Robert Nelson <robertcnelson@gmail.com>
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

test_ti_kernel_version () {
	if [ "x${kernel}" = "x" ] ; then
		major=$(uname -r | awk '{print $1}' | cut -d. -f1)
		minor=$(uname -r | awk '{print $1}' | cut -d. -f2)

		case "x${major}.${minor}x" in
		x4.4x)
			kernel="LTS44"
			;;
		x4.9x)
			kernel="LTS49"
			;;
		x4.14x)
			kernel="LTS414"
			;;
		x4.19x)
			kernel="LTS419"
			;;
		x5.4x)
			kernel="LTS54"
			;;
		esac
	fi
}

scan_ti_kernels () {
	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep ti-xenomai || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="ti-xenomai"
			test_ti_kernel_version
		fi
	fi

	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep ti-rt || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="ti-rt"
			test_ti_kernel_version
		fi
	fi

	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep ti || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="ti"
			test_ti_kernel_version
		fi
	fi
}

test_bone_rt_kernel_version () {
	if [ "x${kernel}" = "x" ] ; then
		major=$(uname -r | awk '{print $1}' | cut -d. -f1)
		minor=$(uname -r | awk '{print $1}' | cut -d. -f2)

		case "x${major}.${minor}x" in
		x4.4x)
			kernel="LTS44"
			;;
		x4.9x)
			kernel="LTS49"
			;;
		x4.14x)
			kernel="LTS414"
			;;
		x4.19x)
			kernel="LTS419"
			;;
		x5.4x)
			kernel="LTS54"
			;;
		esac
	fi
}

test_bone_kernel_version () {
	if [ "x${kernel}" = "x" ] ; then
		major=$(uname -r | awk '{print $1}' | cut -d. -f1)
		minor=$(uname -r | awk '{print $1}' | cut -d. -f2)

		case "x${major}.${minor}x" in
		x3.8x)
			kernel="STABLE"
			;;
		x4.4x)
			kernel="LTS44"
			;;
		x4.9x)
			kernel="LTS49"
			;;
		x4.14x)
			kernel="LTS414"
			;;
		x4.19x)
			kernel="LTS419"
			;;
		x5.4x)
			kernel="LTS54"
			;;
		*)
			#aka STABLE, as 3.8.13 will always be considered STABLE
			kernel="TESTING"
			;;
		esac
	fi
}

scan_bone_kernels () {
	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep bone-rt || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="bone-rt"
			test_bone_rt_kernel_version
		fi
	fi
	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep bone || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="omap-psp"
			test_bone_kernel_version
		fi
	fi
}

test_armv7_kernel_version () {
	if [ "x${kernel}" = "x" ] ; then
		major=$(uname -r | awk '{print $1}' | cut -d. -f1)
		minor=$(uname -r | awk '{print $1}' | cut -d. -f2)

		case "x${major}.${minor}x" in
		x4.4x)
			kernel="LTS44"
			;;
		x4.9x)
			kernel="LTS49"
			;;
		x4.14x)
			kernel="LTS414"
			;;
		x4.19x)
			kernel="LTS419"
			;;
		x5.4x)
			kernel="LTS54"
			;;
		*)
			kernel="STABLE"
			;;
		esac
	fi
}

scan_armv7_kernels () {
	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep lpae || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="armv7-lpae"
			test_armv7_kernel_version
		fi
	fi
	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep armv7-rt || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="armv7-rt"
			test_armv7_kernel_version
		fi
	fi
	if [ "x${SOC}" = "x" ] ; then
		unset testvalue
		testvalue=$(echo ${current_kernel} | grep armv7 || true)
		if [ ! "x${testvalue}" = "x" ] ; then
			SOC="armv7"
			test_armv7_kernel_version
		fi
	fi
}

get_device () {
	if [ "x${FORCEMACHINE}" = "x" ] ; then
		machine=$(cat /proc/device-tree/model | sed "s/ /_/g" | tr -d '\000')
	else
		machine=$(echo ${FORCEMACHINE} | sed "s/ /_/g" | tr -d '\000')
	fi

	if [ "x${SOC}" = "x" ] ; then
		case "${machine}" in
		Arrow_BeagleBone_Black_Industrial)
			scan_ti_kernels
			scan_bone_kernels
			scan_armv7_kernels
			es8="enabled"
			;;
		TI_AM335x_Beagle*)
			scan_ti_kernels
			scan_bone_kernels
			scan_armv7_kernels
			es8="enabled"
			;;
		TI_AM335x_P*)
			scan_ti_kernels
			scan_bone_kernels
			scan_armv7_kernels
			es8="enabled"
			;;
		Octavo_Systems_OSD3358*)
			scan_ti_kernels
			scan_bone_kernels
			scan_armv7_kernels
			es8="enabled"
			;;
		SanCloud_BeagleBone_Enhanced)
			scan_ti_kernels
			scan_bone_kernels
			scan_armv7_kernels
			es8="enabled"
			;;
		TI_AM5728*)
			scan_ti_kernels
			scan_armv7_kernels
			;;
		TI_OMAP5_uEVM_board)
			scan_ti_kernels
			scan_armv7_kernels
			;;
		BeagleBoard.org_BeagleBone_AI)
			scan_ti_kernels
			scan_armv7_kernels
			;;
		*)
			echo "Machine: [${machine}]"
			scan_armv7_kernels
			;;
		esac
	fi

	unset es8
	unset sgxti335x
	unset sgxjacinto6evm
	unset rtl8723bu
	unset rtl8821cu
	unset libpruio
	unset ticmem
	unset tidebugss
	unset titemperature
	unset kernel_headers

	case "${machine}" in
	Arrow_BeagleBone_Black_Industrial)
		libpruio="enabled"
		es8="enabled"
		sgxti335x="enabled"
		;;
	TI_AM335x_BeagleBone*)
		libpruio="enabled"
		es8="enabled"
		sgxti335x="enabled"
		;;
	TI_AM335x_P*)
		libpruio="enabled"
		es8="enabled"
		sgxti335x="enabled"
		;;
	Octavo_Systems_OSD3358*)
		libpruio="enabled"
		es8="enabled"
		sgxti335x="enabled"
		;;
	SanCloud_BeagleBone_Enhanced)
		libpruio="enabled"
		es8="enabled"
		sgxti335x="enabled"
		rtl8723bu="enabled"
		rtl8821cu="enabled"
		;;
	TI_AM5728*)
		sgxjacinto6evm="enabled"
		ticmem="enabled"
		tidebugss="enabled"
		titemperature="enabled"
		;;
	BeagleBoard.org_BeagleBone_AI)
		sgxjacinto6evm="enabled"
		ticmem="enabled"
		;;
	esac
}

update_uEnv_txt () {
	if [ ! -f /etc/kernel/postinst.d/zz-uenv_txt ] ; then
		if [ -f /boot/uEnv.txt ] ; then
			older_kernel=$(grep uname_r /boot/uEnv.txt | awk -F"=" '{print $2}')
			sed -i -e "s:uname_r=$older_kernel:uname_r=$latest_kernel:g" /boot/uEnv.txt
			echo "info: /boot/uEnv.txt: `grep uname_r /boot/uEnv.txt`"
			if [ ! "x${older_kernel}" = "x${latest_kernel}" ] ; then
				echo "info: [${latest_kernel}] now installed and will be used on the next reboot..."
			fi
		fi
	fi

	if [ "x${daily_cron}" = "xenabled" ] ; then
		touch /tmp/daily_cron.reboot
	fi
}

check_dpkg () {
	echo "Checking dpkg..."
	unset deb_pkgs
	LC_ALL=C dpkg --list | awk '{print $2}' | grep "^${pkg}$" >/dev/null || deb_pkgs="${pkg}"
}

check_apt_cache () {
	echo "Checking apt-cache..."
	unset apt_cache
	apt_cache=$(LC_ALL=C apt-cache search "^${pkg}$" | awk '{print $1}' || true)
}

cleanup_old_kernels () {
	if [ "x${cleanup_old_kernels}" = "xenabled" ] ; then
		unset pkg_list
		pkg_list=$(dpkg --list | grep linux-image | awk '{print $2}' | grep -v linux-image-`uname -r` | tr '\n' ' ' || true)
		if [ ! "x${pkg_list}" = "x" ] ; then
			${apt_bin} -y remove --purge ${pkg_list}
		fi
	fi
}

latest_version_repo () {
	if [ ! "x${SOC}" = "x" ] ; then
		cd /tmp/
		if [ -f /tmp/LATEST-${SOC} ] ; then
			rm -f /tmp/LATEST-${SOC} || true
		fi

		echo "info: checking archive"
		wget --no-verbose ${mirror}/${dist}-${arch}/LATEST-${SOC}
		if [ -f /tmp/LATEST-${SOC} ] ; then

			echo "-----------------------------"
			echo "Kernel Options:"
			cat /tmp/LATEST-${SOC} | grep -v LTS314
			echo "-----------------------------"
			echo "Kernel version options:"
			echo "-----------------------------"
			echo "LTS44: --lts-4_4"
			echo "LTS49: --lts-4_9"
			echo "LTS414: --lts-4_14"
			echo "LTS419: --lts-4_19"
			echo "LTS54: --lts-5_4"
			echo "STABLE: --stable"
			echo "TESTING: --testing"
			echo "-----------------------------"

			if [ "x${kernel}" = "x" ] ; then
				echo "Please pass one of the above kernel options to update_kernel.sh"
				echo "-----------------------------"
				exit
			fi

			latest_kernel=$(cat /tmp/LATEST-${SOC} | grep ${kernel} | awk '{print $3}')

			if [ "x${latest_kernel}" = "x" ] ; then
				echo "Please pass one of the above kernel options to update_kernel.sh"
				echo "-----------------------------"
				exit
			fi
			echo "info: you are running: [${current_kernel}], latest is: [${latest_kernel}] updating..."

			if [ "x${current_kernel}" = "x${latest_kernel}" ] ; then
				if [ "x${daily_cron}" = "xenabled" ] ; then
					${apt_bin} clean
					exit
				fi
			fi
			${apt_bin} update || true

			cleanup_old_kernels

			unset flag_reinstall
			pkg="linux-image-${latest_kernel}"
			#is the package installed?
			check_dpkg
			#is the package even available to apt?
			check_apt_cache
			if [ "x${deb_pkgs}" = "x${apt_cache}" ] ; then
				if [ "x${kernel_headers}" = "xenabled" ] ; then
					pkg="${pkg} linux-headers-${latest_kernel}"
				fi
				echo "debug: installing: [${pkg}]"
				${apt_bin} install -y ${pkg}
				update_uEnv_txt
			elif [ "x${pkg}" = "x${apt_cache}" ] ; then
				if [ "x${kernel_headers}" = "xenabled" ] ; then
					pkg="${pkg} linux-headers-${latest_kernel}"
				fi
				echo "debug: reinstalling: [${pkg}]"
				flag_reinstall=true
				${apt_bin} install -y ${pkg} --reinstall
				update_uEnv_txt
			else
				echo "info: [${pkg}] (latest) is currently unavailable on [rcn-ee.com/repos]"
			fi
		fi
	fi
}

#Only for the original May 2014 image, everything should use the repo's now..
latest_version () {
	if [ ! "x${SOC}" = "x" ] ; then
		cd /tmp/
		if [ -f /tmp/LATEST-${SOC} ] ; then
			rm -f /tmp/LATEST-${SOC} || true
		fi

		echo "info: checking archive"
		wget --no-verbose ${mirror}/${dist}-${arch}/LATEST-${SOC}
		if [ -f /tmp/LATEST-${SOC} ] ; then

			latest_kernel=$(cat /tmp/LATEST-${SOC} | grep ${kernel} | awk '{print $3}')

			echo "info: you are running: [${current_kernel}], latest is: [${latest_kernel}] updating..."
			if [ "x${latest_kernel}" = "x" ] ; then
				exit
			fi

			if [ ! "x${current_kernel}" = "x${latest_kernel}" ] ; then
				distro=$(lsb_release -is)
				if [ "x${distro}" = "xDebian" ] ; then
					wget --no-verbose -c https://rcn-ee.com/repos/debian/pool/main/l/linux-upstream/linux-image-${latest_kernel}_1${dist}_${arch}.deb
				else
					wget --no-verbose -c https://rcn-ee.com/repos/ubuntu/pool/main/l/linux-upstream/linux-image-${latest_kernel}_1${dist}_${arch}.deb
				fi
				if [ -f /tmp/linux-image-${latest_kernel}_1${dist}_${arch}.deb ] ; then
					dpkg -i /tmp/linux-image-${latest_kernel}_1${dist}_${arch}.deb
					sync

					if [ -f /boot/vmlinuz-${latest_kernel} ] ; then
						bootdir="/boot/uboot"

						if [ -f ${bootdir}/zImage_bak ] ; then
							rm ${bootdir}/zImage_bak
							sync
						fi

						if [ -f ${bootdir}/zImage ] ; then
							echo "Backing up ${bootdir}/zImage as ${bootdir}/zImage_bak..."
							mv -v ${bootdir}/zImage ${bootdir}/zImage_bak
							sync
						fi

						if [ -f ${bootdir}/initrd.bak ] ; then
							rm ${bootdir}/initrd.bak
							sync
						fi

						if [ -f ${bootdir}/initrd.img ] ; then
							echo "Backing up ${bootdir}/initrd.img as ${bootdir}/initrd.bak..."
							mv -v ${bootdir}/initrd.img ${bootdir}/initrd.bak
							sync
						fi

						if [ -d ${bootdir}/dtbs_bak/ ] ; then
							rm -rf ${bootdir}/dtbs_bak/ || true
							sync
						fi

						if [ -d ${bootdir}/dtbs/ ] ; then
							echo "Backing up ${bootdir}/dtbs/ as ${bootdir}/dtbs_bak/..."
							mv ${bootdir}/dtbs/ ${bootdir}/dtbs_bak/ || true
							sync
						fi

						if [ -d /boot/dtbs/${latest_kernel}/ ] ; then
							mkdir -p ${bootdir}/dtbs/
							cp /boot/dtbs/${latest_kernel}/*.dtb ${bootdir}/dtbs/ 2>/dev/null || true
							sync
						fi

						if [ ! -f /boot/initrd.img-${latest_kernel} ] ; then
							echo "Creating /boot/initrd.img-${latest_kernel}"
							update-initramfs -c -k ${latest_kernel}
							sync
						else
							echo "Updating /boot/initrd.img-${latest_kernel}"
							update-initramfs -u -k ${latest_kernel}
							sync
						fi

						cp -v /boot/vmlinuz-${latest_kernel} ${bootdir}/zImage
						cp -v /boot/initrd.img-${latest_kernel} ${bootdir}/initrd.img
					fi
				fi
			fi
		fi
	fi
}

specific_version_repo () {
	latest_kernel=$(echo ${kernel_version})
	${apt_bin} update || true

	cleanup_old_kernels

	unset flag_reinstall
	pkg="linux-image-${latest_kernel}"
	#is the package installed?
	check_dpkg
	#is the package even available to apt?
	check_apt_cache
	if [ "x${deb_pkgs}" = "x${apt_cache}" ] ; then
		if [ "x${kernel_headers}" = "xenabled" ] ; then
			pkg="${pkg} linux-headers-${latest_kernel}"
		fi
		${apt_bin} install -y ${pkg}
		update_uEnv_txt
	elif [ "x${pkg}" = "x${apt_cache}" ] ; then
		flag_reinstall=true
		${apt_bin} install -y ${pkg} --reinstall
		update_uEnv_txt
	else
		echo "error: [${pkg}] unavailable"
	fi
}

third_party () {
	apt_options="install -y"
	if [ "x${flag_reinstall}" = xtrue ] ; then
		apt_options="install -y --reinstall"
	fi

	case "${SOC}" in
	omap-psp)
		#AKA: "bone"...
		case "${kernel}" in
		STABLE)
			#3.8 only...
			${apt_bin} ${apt_options} -o Dpkg::Options::="--force-overwrite" mt7601u-modules-${latest_kernel} || true
			;;
		LTS44)
			if [ "x${es8}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-es8-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			;;
		LTS414)
			#TESTING|LTS414
			#v4.15.x sgx modules are broken...
			if [ "x${es8}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-es8-modules-${latest_kernel} || true
			fi
			if [ "x${libpruio}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} libpruio-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			;;
		LTS419)
			if [ "x${libpruio}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} libpruio-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			;;
		esac
		;;
	bone-rt)
		case "${kernel}" in
		LTS414)
			#TESTING|LTS414
			#v4.15.x sgx modules are broken...
			if [ "x${es8}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-es8-modules-${latest_kernel} || true
			fi
			if [ "x${libpruio}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} libpruio-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			;;
		LTS419)
			if [ "x${libpruio}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} libpruio-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			;;
		esac
		;;
	ti-xenomai)
		case "${kernel}" in
		LTS44)
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${ticmem}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-cmem-modules-${latest_kernel} || true
			fi
			if [ "x${sgxti335x}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-ti335x-modules-${latest_kernel} || true
			fi
			if [ "x${sgxjacinto6evm}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-jacinto6evm-modules-${latest_kernel} || true
			fi
			;;
		LTS49)
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			if [ "x${ticmem}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-cmem-modules-${latest_kernel} || true
			fi
			if [ "x${sgxti335x}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-ti335x-modules-${latest_kernel} || true
			fi
			if [ "x${sgxjacinto6evm}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-jacinto6evm-modules-${latest_kernel} || true
			fi
			;;
		LTS414)
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			if [ "x${libpruio}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} libpruio-modules-${latest_kernel} || true
			fi
			if [ "x${ticmem}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-cmem-${cmem_version}-modules-${latest_kernel} || true
			fi
			if [ "x${sgxti335x}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-ti335x-modules-${latest_kernel} || true
				if [ "x${sgx_blob}" = "xenabled" ] ; then
					${apt_bin} ${apt_options} ti-sgx-common-ddk-um || true
					${apt_bin} ${apt_options} ti-sgx-ti33x-ddk-um || true
				fi
			fi
			if [ "x${sgxjacinto6evm}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-jacinto6evm-modules-${latest_kernel} || true
			fi
			;;
		esac
		;;
	ti|ti-rt)
		case "${kernel}" in
		LTS44)
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${ticmem}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-cmem-modules-${latest_kernel} || true
			fi
			if [ "x${sgxti335x}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-ti335x-modules-${latest_kernel} || true
			fi
			if [ "x${sgxjacinto6evm}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-jacinto6evm-modules-${latest_kernel} || true
			fi
			;;
		LTS49)
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			if [ "x${ticmem}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-cmem-modules-${latest_kernel} || true
			fi
			if [ "x${sgxti335x}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-ti335x-modules-${latest_kernel} || true
			fi
			if [ "x${sgxjacinto6evm}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-jacinto6evm-modules-${latest_kernel} || true
			fi
			;;
		LTS414)
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			if [ "x${libpruio}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} libpruio-modules-${latest_kernel} || true
			fi
			if [ "x${ticmem}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-cmem-${cmem_version}-modules-${latest_kernel} || true
			fi
			if [ "x${sgxti335x}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-ti335x-modules-${latest_kernel} || true
				if [ "x${sgx_blob}" = "xenabled" ] ; then
					${apt_bin} ${apt_options} ti-sgx-common-ddk-um || true
					${apt_bin} ${apt_options} ti-sgx-ti33x-ddk-um || true
				fi
			fi
			if [ "x${sgxjacinto6evm}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-jacinto6evm-modules-${latest_kernel} || true
			fi
			;;
		LTS419)
			cmem_version="4.16.00.00"
			if [ "x${rtl8723bu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8723bu-modules-${latest_kernel} || true
			fi
			if [ "x${rtl8821cu}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} rtl8821cu-modules-${latest_kernel} || true
			fi
			if [ "x${libpruio}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} libpruio-modules-${latest_kernel} || true
			fi
			if [ "x${ticmem}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-cmem-${cmem_version}-modules-${latest_kernel} || true
			fi
			if [ "x${sgxti335x}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-ti335x-modules-${latest_kernel} || true
				if [ "x${sgx_blob}" = "xenabled" ] ; then
					${apt_bin} ${apt_options} ti-sgx-common-ddk-um || true
					${apt_bin} ${apt_options} ti-sgx-ti33x-ddk-um || true
				fi
			fi
			if [ "x${sgxjacinto6evm}" = "xenabled" ] ; then
				${apt_bin} ${apt_options} ti-sgx-jacinto6evm-modules-${latest_kernel} || true
			fi
			;;
		esac
		;;
	esac

	depmod -a ${latest_kernel}
	update-initramfs -uk ${latest_kernel}
}

checkparm () {
	if [ "$(echo $1|grep ^'\-')" ] ; then
		echo "E: Need an argument"
		exit
	fi
}

get_dist=$(cat /etc/apt/sources.list | grep -v deb-src | grep armhf | grep repos.rcn-ee.com | head -1 | awk '{print $4}' || true)
case "${get_dist}" in
wheezy|jessie)
	dist="${get_dist}"
	apt_bin="apt-get"
	cmem_version="4.15.00.02"
	;;
stretch)
	dist="${get_dist}"
	apt_bin="apt"
	cmem_version="4.15.00.02"
	;;
buster|sid)
	dist="${get_dist}"
	apt_bin="apt"
	cmem_version="4.16.00.00"
	;;
bionic|cosmic|disco)
	dist="${get_dist}"
	apt_bin="apt"
	cmem_version="4.16.00.00"
	;;
trusty|utopic|vivid|wily|xenial|yakkety|artful)
	dist="${get_dist}"
	apt_bin="apt-get"
	cmem_version="4.15.00.02"
	;;
*)
	dist=""
	apt_bin="apt-get"
	cmem_version="4.15.00.02"
	;;
esac

if [ "x${dist}" = "x" ] ; then
	if [ ! -f /usr/bin/lsb_release ] ; then
		echo "install lsb-release"
		echo "sudo ${apt_bin} install lsb-release"
		exit
	fi

	dist=$(lsb_release -cs | sed 's/\//_/g')
fi
arch=$(dpkg --print-architecture)
current_kernel=$(uname -r)

#Debian testing...
if [ "x${dist}" = "xn_a" ] ; then
	deb_lsb_rs=$(lsb_release -rs | awk '{print $1}' | sed 's/\//_/g')

	#Distributor ID:	Debian
	#Description:	Debian GNU/Linux testing/unstable
	#Release:	testing/unstable
	#Codename:	n/a

	if [ "x${deb_lsb_rs}" = "xtesting_unstable" ] ; then
		dist="stretch"
	fi
fi

unset kernel
mirror="https://rcn-ee.com/repos/latest"
unset kernel_version
unset daily_cron
unset old_rootfs
unset sgx_blob
# parse commandline options
while [ ! -z "$1" ] ; do
	case $1 in
	--kernel)
		checkparm $2
		kernel_version="$2"
		;;
	--daily-cron)
		daily_cron="enabled"
		;;
	--lts-3_14-kernel|--lts-3_14)
		kernel="LTS44"
		;;
	--lts-kernel|--lts-4_1-kernel|--lts-4_1)
		kernel="LTS44"
		;;
	--lts-4_4-kernel|--lts-4_4|--lts)
		kernel="LTS44"
		;;
	--lts-4_9-kernel|--lts-4_9)
		kernel="LTS49"
		;;
	--lts-4_14-kernel|--lts-4_14)
		kernel="LTS414"
		;;
	--lts-4_19-kernel|--lts-4_19)
		kernel="LTS419"
		;;
	--lts-5_4-kernel|--lts-5_4)
		kernel="LTS54"
		;;
	--stable-kernel|--stable)
		kernel="STABLE"
		;;
	--beta-kernel|--beta|--testing-kernel|--testing)
		kernel="TESTING"
		;;
	--exp-kernel|--exp)
		kernel="EXPERIMENTAL"
		;;
	--armv7-channel)
		SOC="armv7"
		;;
	--armv7-rt-channel)
		SOC="armv7-rt"
		;;
	--armv7-lpae-channel)
		SOC="armv7-lpae"
		;;
	--bone-kernel|--bone-channel)
		SOC="omap-psp"
		;;
	--bone-rt-kernel|--bone-rt-channel)
		SOC="bone-rt"
		;;
	--bone-xenomai-kernel|--bone-xenomai-channel)
		SOC="xenomai"
		kernel="STABLE"
		;;
	--imxv6v7-channel)
		SOC="imxv6v7"
		kernel="STABLE"
		;;
	--multiv7-channel)
		SOC="multiv7"
		kernel="STABLE"
		;;
	--omap2plus-channel|--ti-omap2plus-channel)
		SOC="omap2plus"
		kernel="STABLE"
		;;
	--tegra-channel)
		SOC="tegra"
		kernel="STABLE"
		;;
	--ti-kernel|--ti-channel)
		SOC="ti"
		;;
	--ti-rt-kernel|--ti-rt-channel)
		SOC="ti-rt"
		;;
	--ti-xenomai-kernel|--ti-xenomai-channel)
		SOC="ti-xenomai"
		;;
	--pre-fall-2014-rootfs)
		old_rootfs="enabled"
		;;
	--cleanup-old-kernels)
		cleanup_old_kernels="enabled"
		;;
	--sgx)
		sgx_blob="enabled"
		;;
	esac
	shift
done


if [ ! -f /lib/systemd/system/systemd-timesyncd.service ] ; then
	if [ -f /usr/sbin/ntpdate ] ; then
		echo "syncing local clock to pool.ntp.org"
		ntpdate -s pool.ntp.org || true
	fi
fi

test_rcnee=$(cat /etc/apt/sources.list | grep repos.rcn-ee || true)
if [ ! "x${test_rcnee}" = "x" ] && [ "x${old_rootfs}" = "x" ] ; then
	net_rcnee=$(cat /etc/apt/sources.list | grep repos.rcn-ee.net || true)
	if [ ! "x${net_rcnee}" = "x" ] ; then
		sed -i -e 's:repos.rcn-ee.net:repos.rcn-ee.com:g' /etc/apt/sources.list
	fi
	get_device

	if [ "x${kernel_version}" = "x" ] ; then
		latest_version_repo
	else
		specific_version_repo
	fi
	third_party
	${apt_bin} clean
else
	get_device
	latest_version
fi
#third_party
#
