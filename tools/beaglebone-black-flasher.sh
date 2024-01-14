#!/bin/bash -e

if ! id | grep -q root; then
	echo "must be run as root"
	exit
fi

kiwisdr_message="D11+ BB/BBAI 1/11/2024 tools/beaglebone-black-flasher.sh"
version_message="1.20231207.0, e2fsprogs (1.47.0) disable orphan_file..."

# set in /etc/default/beagle-flasher
#debug_over_display=tty0
#source=/dev/mmcblk1
#destination=/dev/mmcblk0
#rfs_partition=single
#rfs_rootfs_type=ext4
#rfs_rootfs_startmb=4
#bootloader=/opt/u-boot/bb-u-boot-am335x-evm/install-mmcblk0.sh
#flash_back=beaglebone-black-microsd-to-emmc

#Debian 12, Ubuntu Lunar, Mantic, Noble
#https://packages.debian.org/source/bookworm/e2fsprogs
#e2fsprogs (1.47.0) added orphan_file first added in v5.15.x
ext4_ver=$(mkfs.ext4 -V |& grep mke2fs | awk '{print $2}')
function version_dot3 { echo "$@" | awk -F. '{ printf("%d%03d%03d\n", $1,$2,$3); }'; }
if [ $(version_dot3 $ext4_ver) -ge $(version_dot3 "1.47.0") ] ; then
    ext4_options="-O ^orphan_file"
else
    ext4_options=""
fi
echo "ext4_ver=$ext4_ver ext4_options=$ext4_options"

# cmdline functionality in /boot/uEnv.txt moving to /boot/firmware/extlinux/extlinux.conf
enable_flasher() {
    uenv=/tmp/rootfs/boot/uEnv.txt
    extl=/tmp/rootfs/boot/firmware/extlinux/extlinux.conf
    
	if [ -f ${uenv} ] ; then
        message="INFO: enable sd card as flasher"                                                  ; broadcast
        sed -i -e 's:#cmdline=init:cmdline=init:g' ${uenv}
        message="`grep cmdline=init ${uenv} || true`"                                              ; broadcast
        message="--------------------------------------------------------------------------------" ; broadcast
        message="INFO: [cat ${uenv}]"                                                              ; broadcast
        message="`cat ${uenv} || true`"                                                            ; broadcast
        message="--------------------------------------------------------------------------------" ; broadcast

	elif [ -f ${extl} ] ; then

        # KiwiSDR: this is from /usr/sbin/enable-beagle-flasher
        message="INFO: enable sd card as flasher"                                                  ; broadcast
        sed -i -e 's:net.ifnames=0 quiet:net.ifnames=0 quiet init=/usr/sbin/init-beagle-flasher:g' ${extl}
        systemctl disable grow_partition.service || true
        message="`grep init= ${extl} || true`"                                                     ; broadcast
        message="--------------------------------------------------------------------------------" ; broadcast

        message="INFO: [cat ${extl}]"                                                              ; broadcast
        message="`cat ${extl} || true`"                                                            ; broadcast
        message="--------------------------------------------------------------------------------" ; broadcast
    else
        message="ERROR: sd card flasher enable failed!"                                            ; broadcast
        message="--------------------------------------------------------------------------------" ; broadcast
        err=89; failure
    fi

    echo `date` >/tmp/rootfs/root/_FLASHED_FROM_SD_
    echo $kiwisdr_message >>/tmp/rootfs/root/_FLASHED_FROM_SD_
    echo $version_message >>/tmp/rootfs/root/_FLASHED_FROM_SD_
}

cylon_leds() {
	#ls /sys/class/leds/beaglebone\:green\:usr*
	#cat /sys/class/leds/beaglebone\:green\:usr*/trigger
	local leds_base=/sys/class/leds/beaglebone\:green\:usr
	if [ -e ${leds_base}0/trigger ] ; then
		echo none > ${leds_base}0/trigger || true
		echo none > ${leds_base}1/trigger || true
		echo none > ${leds_base}2/trigger || true
		echo none > ${leds_base}3/trigger || true

		STATE=1
		while : ; do
		case $STATE in
			1)
				echo 1 > ${leds_base}0/brightness || true
				echo 0 > ${leds_base}1/brightness || true
				STATE=2
				;;
			2)
				echo 1 > ${leds_base}1/brightness || true
				echo 0 > ${leds_base}0/brightness || true
				STATE=3
				;;
			3)
				echo 1 > ${leds_base}2/brightness || true
				echo 0 > ${leds_base}1/brightness || true
				STATE=4
				;;
			4)
				echo 1 > ${leds_base}3/brightness || true
				echo 0 > ${leds_base}2/brightness || true
				STATE=5
				;;
			5)
				echo 1 > ${leds_base}2/brightness || true
				echo 0 > ${leds_base}3/brightness || true
				STATE=6
				;;
			6)
				echo 1 > ${leds_base}1/brightness || true
				echo 0 > ${leds_base}2/brightness || true
				STATE=1
				;;
			*)
				echo 1 > ${leds_base}0/brightness || true
				echo 0 > ${leds_base}1/brightness || true
				STATE=2
				;;
		esac
		sleep 0.1
		done
	fi
}

reset_leds() {
	if [ "x${has_usr_leds}" = "xenable" ] ; then
		if [ -e /proc/$CYLON_PID ]; then
			#kill $CYLON_PID > /dev/null 2>&1
		    message="INFO: [kill $CYLON_PID]"                                                                 ; broadcast
			kill $CYLON_PID
		fi

		leds_pattern0=${leds_pattern0:-"heartbeat"}
		leds_pattern1=${leds_pattern1:-"mmc0"}
		leds_pattern2=${leds_pattern2:-"cpu0"}
		leds_pattern3=${leds_pattern3:-"mmc1"}
		leds_base=/sys/class/leds/beaglebone\:green\:usr

		if [ -e ${leds_base}0/trigger ] ; then
			echo ${leds_pattern0} > ${leds_base}0/trigger || true
			echo ${leds_pattern1} > ${leds_base}1/trigger || true
			echo ${leds_pattern2} > ${leds_base}2/trigger || true
			echo ${leds_pattern3} > ${leds_base}3/trigger || true
		fi
	fi
}

error_leds() {
	if [ "x${has_usr_leds}" = "xenable" ] ; then
		if [ -e /proc/$CYLON_PID ]; then
			kill $CYLON_PID > /dev/null 2>&1
		fi

		leds_pattern0=${leds_pattern0:-"heartbeat"}
		leds_pattern1=${leds_pattern1:-"heartbeat"}
		leds_pattern2=${leds_pattern2:-"heartbeat"}
		leds_pattern3=${leds_pattern3:-"heartbeat"}
		leds_base=/sys/class/leds/beaglebone\:green\:usr

		if [ -e ${leds_base}0/trigger ] ; then
			echo ${leds_pattern0} > ${leds_base}0/trigger || true
			echo ${leds_pattern1} > ${leds_base}1/trigger || true
			echo ${leds_pattern2} > ${leds_base}2/trigger || true
			echo ${leds_pattern3} > ${leds_base}3/trigger || true
		fi
	fi
}

flush_cache () {
	sync
	if [ "x${destination}" != "x" ] ; then
		message="INFO: flush_cache: [blockdev --flushbufs ${destination}]"                         ; broadcast
		blockdev --flushbufs ${destination} || true
		message="--------------------------------------------------------------------------------" ; broadcast
	fi
}

broadcast () {
	if [ "x${message}" != "x" ] ; then
		echo "${message}"
		if [ "x${debug_over_display}" != "x" ] ; then
			echo "${message}" > /dev/${debug_over_display} || true
		fi
	fi
}

broadcast_over_display () {
	if [ "x${message}" != "x" ] ; then
		if [ "x${debug_over_display}" != "x" ] ; then
			echo "${message}" > /dev/${debug_over_display} || true
		fi
	fi
}

failure () {
	message="error code ${err}" ; broadcast
    reset_leds
	exit ${err}
}

format_failure () {
	message="ERROR: formatting [destination=${destination}] failed..."                         ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache
	if [ "x${destination}" != "x" ] ; then
		umount ${destination}p1 > /dev/null 2>&1 || true
	fi
	
	err=31; failure
}

write_failure () {
	message="ERROR: writing to [destination=${destination}] failed..."                         ; broadcast
	error_leds
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache
	if [ "x${destination}" != "x" ] ; then
		umount ${destination}p1 > /dev/null 2>&1 || true
	fi
	
	err=15; failure
}

# KiwiSDR: needed if an admin page reload restarts script
unmount_previous () {
    NUM_MOUNTS=$(mount | grep -v none | grep "${destination}" | wc -l)

    i=0 ; while test $i -lt ${NUM_MOUNTS} ; do
        DRIVE=$(mount | grep -v none | grep "${destination}" | tail -1 | awk '{print $1}')
		message="INFO: Unmount ${DRIVE}"                                                           ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
        umount ${DRIVE} >/dev/null 2>&1 || true
        i=$(($i+1))
    done
}

check_running_system () {

	message="INFO: /proc/cmdline"                                                              ; broadcast
	message="INFO: [cat /proc/cmdline]"                                                        ; broadcast
	cat /proc/cmdline
	message="--------------------------------------------------------------------------------" ; broadcast

	has_usr_leds="enable"
	machine=$(cat /proc/device-tree/model | sed "s/ /_/g" | tr -d '\000')

	case "${machine}" in
		TI_AM5728_BeagleBoard*)
			unset has_usr_leds
			;;
	esac

	if [ -f /proc/sys/vm/min_free_kbytes ] ; then
		message="INFO: Preparing sysctl"                                                           ; broadcast
		value_min_free_kbytes=$(sysctl -n vm.min_free_kbytes)
		message="INFO: [sysctl: vm.min_free_kbytes=[${value_min_free_kbytes}]"                     ; broadcast
		message="INFO: [sysctl: setting: [sysctl -w vm.min_free_kbytes=16384]"                     ; broadcast
		sysctl -w vm.min_free_kbytes=16384
		message="--------------------------------------------------------------------------------" ; broadcast
	fi

    # KiwiSDR: limit size of backup partition
	kiwi_rootfs_size="`/bin/df --block-size=1048576 . | tail -n 1 | awk '{print $3}'`"
	kiwi_rootfs_size=$((${kiwi_rootfs_size}+200))
	message="setting micro-SD partition size to actual rootfs size + 200M = ${kiwi_rootfs_size}M"       ; broadcast
	message="rootfs_size ${kiwi_rootfs_size}M"                                                          ; broadcast
    message="--------------------------------------------------------------------------------" ; broadcast

	if [ "x${has_usr_leds}" = "xenable" ] ; then
		cylon_leds & CYLON_PID=$!
		echo "CYLON_PID=$CYLON_PID"
	fi

	if [ "x${source}" != "x" ] && [ "x${destination}" != "x" ] ; then
		message="INFO: [lsblk -i]"                                                                 ; broadcast
		message="`lsblk -i || true`"                                                               ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
		message="INFO: copying [${source}] -> [${destination}]"                                    ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
	else
		message="INFO: [lsblk -i]"                                                                 ; broadcast
		message="`lsblk -i || true`"                                                               ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
		message="ERROR: Setup: [source] and [destination] in /etc/default/beagle-flasher"          ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
		failure
	fi

	if [ ! -b "${source}" ] ; then
		message="ERROR: [source=${source}] does not exist"                                         ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
		failure
	else
		message="INFO: [source=${source}] is a valid block device"                                 ; broadcast
	fi

	if [ ! -b "${destination}" ] ; then
		message="ERROR: [destination=${destination}] does not exist"                               ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
		err=1; failure
	else
		message="INFO: [destination=${destination}] is a valid block device"                       ; broadcast
	fi

    unmount_previous
    
	if [ ! -f /boot/config-$(uname -r) ] ; then
		zcat /proc/config.gz > /boot/config-$(uname -r) || true
		message="INFO: Creating: [/boot/config-$(uname -r)]"                                       ; broadcast
	else
		message="INFO: [/boot/config-$(uname -r)]"                                                 ; broadcast
	fi

	if [ -f /usr/sbin/update-initramfs ] ; then
		message="INFO: Generating: [/boot/initrd.img-$(uname -r)]"                                         ; broadcast
		message="update-initramfs: Generating /boot/initrd.img-$(uname -r)"                                ; broadcast_over_display
		if [ -f /boot/initrd.img-$(uname -r) ] ; then
			/usr/sbin/update-initramfs -u -k $(uname -r) || true
			message="--------------------------------------------------------------------------------" ; broadcast
		else
			/usr/sbin/update-initramfs -c -k $(uname -r) || true
			message="--------------------------------------------------------------------------------" ; broadcast
		fi
	fi
	flush_cache
}

format_boot () {
	message="/sbin/mkfs.vfat -F 16 ${destination}p1 -n ${boot_label}"                          ; broadcast
	LC_ALL=C /sbin/mkfs.vfat -F 16 ${destination}p1 -n ${boot_label} || format_failure
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache
}

format_root () {
	message="/sbin/mkfs.ext4 ${ext4_options} ${destination}p2 -L ${rootfs_label}"              ; broadcast
	LC_ALL=C /sbin/mkfs.ext4 ${ext4_options} ${destination}p2 -L ${rootfs_label} || format_failure
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache
}

format_single_root () {
	message="/sbin/mkfs.ext4 ${ext4_options} ${destination}p1 -L ${single_root_label}"         ; broadcast
	LC_ALL=C /sbin/mkfs.ext4 ${ext4_options} ${destination}p1 -L ${single_root_label} || format_failure
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache
}

copy_boot () {
	message="Copying: ${source}p1 -> ${destination}p1" ; broadcast
	mkdir -p /tmp/boot/ || true

	mount ${destination}p1 /tmp/boot/ -o sync

	message="rsync: (-aHAXx) /boot/firmware/ -> /tmp/boot/" ; broadcast
	rsync -aHAXx /boot/firmware/ /tmp/boot/ || write_failure
	flush_cache

	umount /tmp/boot/ || umount -l /tmp/boot/ || write_failure
	flush_cache
}

copy_rootfs () {
	message="INFO: Copying: ${source}p${media_rootfs} -> ${destination}p${media_rootfs}"       ; broadcast
	mkdir -p /tmp/rootfs/ || true

	message="INFO: [mount ${destination}p${media_rootfs} /tmp/rootfs/ -o async,noatime]"       ; broadcast
	mount ${destination}p${media_rootfs} /tmp/rootfs/ -o async,noatime

    tr="/usr/bin/tr '\\r' '\\n'"
    throttle="/root/Beagle_SDR_GPS/tools/throttle 3"

	message="INFO: /usr/bin/rsync: (-aHAXx) [/ -> /tmp/rootfs/]"                               ; broadcast
	message="INFO: rsync: note the % column is useless..."                                     ; broadcast
	( /usr/bin/rsync -aHAXx --human-readable --info=name0,progress2 /* /tmp/rootfs/ --exclude={/dev/*,/proc/*,/sys/*,/tmp/*,/run/*,/mnt/*,/media/*,/lost+found,/lib/modules/*,/uEnv.txt} | ${tr} | ${throttle} ) || write_failure
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache
	message="rootfs_size 1M" ; broadcast

	message="INFO: Copying: Kernel modules"                                                    ; broadcast
	mkdir -p /tmp/rootfs/lib/modules/$(uname -r)/ || true
	message="INFO: /usr/bin/rsync: (-aHAXx) /lib/modules/$(uname -r)/ -> /tmp/rootfs/lib/modules/$(uname -r)/" ; broadcast
	/usr/bin/rsync -aHAXx /lib/modules/$(uname -r)/* /tmp/rootfs/lib/modules/$(uname -r)/ || write_failure
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache

	message="INFO: Generating: /etc/fstab"                                                     ;  broadcast
	echo "# /etc/fstab: static file system information." > /tmp/rootfs/etc/fstab
	echo "#" >> /tmp/rootfs/etc/fstab
	echo "${destination}p${media_rootfs}  /  ext4  noatime,errors=remount-ro  0  1" >> /tmp/rootfs/etc/fstab
	if [ "x${media_rootfs}" = "x2" ] ; then
		echo "${destination}p1  /boot/firmware vfat defaults 0 0" >> /tmp/rootfs/etc/fstab
	fi
	echo "debugfs  /sys/kernel/debug  debugfs  mode=755,uid=root,gid=gpio,defaults  0  0" >> /tmp/rootfs/etc/fstab
	message="INFO: [cat /tmp/rootfs/etc/fstab]"                                                ; broadcast
	message="`cat /tmp/rootfs/etc/fstab`"                                                      ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache

    enable_flasher

	if [ "x${flash_back}" != "x" ] ; then
		if [ -f ${flash_back} ] ; then
			message="INFO: [cp -v ${flash_back} /tmp/rootfs/etc/default/beagle-flasher]" ; broadcast
			cp -v ${flash_back} /tmp/rootfs/etc/default/beagle-flasher
			message="--------------------------------------------------------------------------------" ; broadcast
		else
			if [ -f /etc/beagle-flasher/${flash_back} ] ; then
				message="INFO: [cp -v /etc/beagle-flasher/${flash_back} /tmp/rootfs/etc/default/beagle-flasher]" ; broadcast
				cp -v /etc/beagle-flasher/${flash_back} /tmp/rootfs/etc/default/beagle-flasher
				message="--------------------------------------------------------------------------------" ; broadcast
			fi
		fi
	fi

	if [ -f /tmp/rootfs/etc/default/generic-sys-mods ] ; then
		. /tmp/rootfs/etc/default/generic-sys-mods

		echo "#This file is sourced by /usb/bin/bb-growpart" > /tmp/rootfs/etc/default/generic-sys-mods
		echo "ROOT_DRIVE=${destination}" >> /tmp/rootfs/etc/default/generic-sys-mods
		echo "ROOT_PARTITION=${ROOT_PARTITION}" >> /tmp/rootfs/etc/default/generic-sys-mods
		echo "ARCH_SOC_MODULES=${ARCH_SOC_MODULES}" >> /tmp/rootfs/etc/default/generic-sys-mods

		message="INFO: [cat /tmp/rootfs/etc/default/generic-sys-mods]"                             ; broadcast
		message="`cat /tmp/rootfs/etc/default/generic-sys-mods`"                                   ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast
	fi

	if [ -f /tmp/rootfs/etc/systemd/system/multi-user.target.wants/grow_partition.service ] ; then
		rm -rf /tmp/rootfs/etc/systemd/system/multi-user.target.wants/grow_partition.service || true
	fi
	flush_cache

	message="Copying: ${source}p${media_rootfs} -> ${destination}p${media_rootfs} complete"    ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache

	message="INFO: [umount /tmp/rootfs/ || umount -l /tmp/rootfs/ || write_failure]"           ; broadcast
	umount /tmp/rootfs/ || umount -l /tmp/rootfs/ || write_failure
	message="--------------------------------------------------------------------------------" ; broadcast

	message="INFO: Force writeback of eMMC buffers by Syncing: ${destination}"                 ; broadcast
	message="INFO: [dd if=${destination} of=/dev/null count=100000 status=progress]"           ; broadcast
	dd if=${destination} of=/dev/null count=100000 status=progress
	message="--------------------------------------------------------------------------------" ; broadcast
	flush_cache
    reset_leds
}

partition_drive () {
	message="INFO: Erasing: [${destination}]"                                                  ; broadcast
	flush_cache
	message="INFO: [dd if=/dev/zero of=${destination} bs=1M count=108 status=progress]"        ; broadcast
	dd if=/dev/zero of=${destination} bs=1M count=108 status=progress
	message="--------------------------------------------------------------------------------" ; broadcast
	sync
	message="INFO: [dd if=${destination} of=/dev/null bs=1M count=108 status=progress]"        ; broadcast
	dd if=${destination} of=/dev/null bs=1M count=108 status=progress
	message="--------------------------------------------------------------------------------" ; broadcast
	sync
	flush_cache
	message="INFO: Erasing: [${destination}] complete"                                         ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast

	if [ "x${bootloader}" != "x" ] ; then
		if [ -f ${bootloader} ] ; then
			message="INFO: [/bin/bash ${bootloader}]"                                                  ; broadcast
			/bin/bash ${bootloader}
			message="--------------------------------------------------------------------------------" ; broadcast
		fi
	fi

	if [ "x${rfs_partition}" = "xsingle" ] ; then
		rfs_rootfs_startmb=${rfs_rootfs_startmb:-"4"}
		rfs_rootfs_sizemb=${kiwi_rootfs_size:-""}
		sfdisk_fstype=${sfdisk_fstype:-"L"}

		single_root_label=${single_root_label:-"rootfs"}

		sfdisk_options="--force --wipe-partitions always"

		message="INFO: Partitioning: ${destination}"                                               ; broadcast
		message="INFO: /sbin/sfdisk: [$(LC_ALL=C /sbin/sfdisk --version)]"                         ; broadcast
		message="INFO: /sbin/sfdisk: [/sbin/sfdisk ${sfdisk_options} ${destination}]"              ; broadcast
		message="INFO: /sbin/sfdisk: [${rfs_rootfs_startmb}M,${rfs_rootfs_sizemb}M,${sfdisk_fstype},*]" ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast

		LC_ALL=C /sbin/sfdisk ${sfdisk_options} "${destination}" <<-__EOF__
			${rfs_rootfs_startmb}M,${rfs_rootfs_sizemb}M,${sfdisk_fstype},*
		__EOF__

		flush_cache
		message="INFO: Formatting: ${destination}"                                                 ; broadcast
		format_single_root
		message="INFO: Formatting: ${destination} complete"                                        ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast

		media_rootfs="1"
		copy_rootfs
	elif [ "x${rfs_partition}" = "xdual" ] ; then

        # KiwiSDR
        message="ERROR: this script doesn't support dual partitions!"                              ; broadcast
        message="--------------------------------------------------------------------------------" ; broadcast
		err=99; failure
        
		rfs_boot_startmb=${rfs_boot_startmb:-"1"}
		rfs_boot_size_mb=${rfs_boot_size_mb:-"64"}
		rfs_sfdisk_fstype=${rfs_sfdisk_fstype:-"0xc"}

		sfdisk_rootfs_startmb=$(($rfs_boot_startmb + $rfs_boot_size_mb))
		sfdisk_rootfs_sizemb=${kiwi_rootfs_size:-""}

		boot_label=${boot_label:-"FIRMWARE"}
		rootfs_label=${rootfs_label:-"rootfs"}

		sfdisk_options="--force --wipe-partitions always"

		message="INFO: Partitioning: ${destination}"                                               ; broadcast
		message="INFO: /sbin/sfdisk: [$(LC_ALL=C /sbin/sfdisk --version)]"                         ; broadcast
		message="INFO: /sbin/sfdisk: [/sbin/sfdisk ${sfdisk_options} ${destination}]"              ; broadcast
		message="INFO: /sbin/sfdisk: [${rfs_boot_startmb}M,${rfs_boot_size_mb}M,${rfs_sfdisk_fstype},*]" ; broadcast
		message="INFO: /sbin/sfdisk: [${sfdisk_rootfs_startmb}M,${sfdisk_rootfs_sizemb}M,,-]"      ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast

		LC_ALL=C /sbin/sfdisk ${sfdisk_options} "${destination}" <<-__EOF__
			${rfs_boot_startmb}M,${rfs_boot_size_mb}M,${rfs_sfdisk_fstype},*
			${sfdisk_rootfs_startmb}M,${sfdisk_rootfs_sizemb}M,,-
		__EOF__

		flush_cache
		message="INFO: Formatting: ${destination}"                                                 ; broadcast
		format_boot
		format_root
		message="INFO: Formatting: ${destination} complete"                                        ; broadcast
		message="--------------------------------------------------------------------------------" ; broadcast

		copy_boot
		media_rootfs="2"
		copy_rootfs
	else
		err=30; failure
	fi
}

if [ -f /etc/default/beagle-flasher ] ; then
	. /etc/default/beagle-flasher
	message="--------------------------------------------------------------------------------" ; broadcast
	message="KiwiSDR: [${kiwisdr_message}]"                                                    ; broadcast
	message="Version: [${version_message}]"                                                    ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast
	message="cat /etc/default/beagle-flasher:"                                                 ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast
	message="`cat /etc/default/beagle-flasher`"                                                ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast
else
	message="--------------------------------------------------------------------------------" ; broadcast
	message="KiwiSDR: [${kiwisdr_message}]"                                                    ; broadcast
	message="Version: [${version_message}]"                                                    ; broadcast
	message="--------------------------------------------------------------------------------" ; broadcast
    message="ERROR: /etc/default/beagle-flasher doesn't exist!"                                ; broadcast
    message="--------------------------------------------------------------------------------" ; broadcast
	failure
fi

err=2
check_running_system
partition_drive
