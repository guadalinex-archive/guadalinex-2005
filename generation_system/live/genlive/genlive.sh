#!/bin/bash
#
# FIXME: very initial version, needs a lot of comprobations and so on.

KERNEL="2.6.12-9-386"
SOURCES="/media/distro/sources"
MASTER="/media/distro/master"
ISOS="/media/distro/isos"
LOG_FILE="/var/log/installer.log"

if [ ! -d ${MASTER}/isolinux ]; then
	mkdir -p ${MASTER}/isolinux
fi

if [ ! -d ${MASTER}/META ]; then
	mkdir -p ${MASTER}/META
fi

cp -a /usr/lib/syslinux/isolinux.bin ${MASTER}/isolinux/
cp -a /usr/share/genlive/isolinux/* ${MASTER}/isolinux/
cp -a /boot/vmlinuz-${KERNEL} ${MASTER}/isolinux/vmlinuz
mkinitramfs -o ${MASTER}/isolinux/initramfs ${KERNEL}

if [ "$1" == "-x" ] && [ -e ${MASTER}/META/META.squashfs ]; then
        echo "${MASTER}/META/META.squashfs already exist. Skipping mksquashfs..."
else        
        mksquashfs ${SOURCES} ${MASTER}/META/META.squashfs
fi

mkisofs -l -r -J -V "Guadalinex Live System" -hide-rr-moved -v -b isolinux/isolinux.bin -c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -o ${ISOS}/gl2005-$(date +%Y%m%d%H%M).iso ${MASTER}
