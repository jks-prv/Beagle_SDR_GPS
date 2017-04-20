#!/bin/sh
format=1.0

board=am335x_evm

bootloader_location=dd_spl_uboot_boot
bootrom_gpt=

dd_spl_uboot_count=1
dd_spl_uboot_seek=1
dd_spl_uboot_conf=notrunc
dd_spl_uboot_bs=128k
dd_spl_uboot_backup=/opt/backup/uboot/MLO

dd_uboot_count=2
dd_uboot_seek=1
dd_uboot_conf=notrunc
dd_uboot_bs=384k
dd_uboot_backup=/opt/backup/uboot/u-boot.img

boot_fstype=ext4
conf_boot_startmb=1
sfdisk_fstype=L

boot_label=KIWISDR
rootfs_label=rootfs

#Kernel
dtb=
serial_tty=ttyO0
usbnet_mem=
