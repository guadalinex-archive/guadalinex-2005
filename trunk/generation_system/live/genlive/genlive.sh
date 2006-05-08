#!/bin/bash
#
# FIXME: very initial version, needs a lot of comprobations and so on.

KERNEL="2.6.12-9-386"
SOURCES="/media/distro/sources"
MASTER="/media/distro/master"
ISOS="/media/distro/isos"
IMAGEPREFIX="gl_v3"
OUTPUTIMAGE=${ISOS}/${IMAGEPREFIX}-$(date +%Y%m%d%H%M).iso
LOG_FILE="/var/log/installer.log"
SPLASH_IMAGE=""
VOLUMENAME="Guadalinex Live System"

function usage() {
cat <<EOF
Usage:
	$0 [-h] [-s <splash image>] [-v <volume name>] [-o <output image>] [-p <image prefix>] [-k <kernel version>] [-x] [-y]

Options:
	-h  ver las opciones
	-x  no regenera la imagen comprimida de la distribución
	-y  no regenera el initramfs
	-k <kernel version> especificar una versión de kernel. Se sustituirá la versión predeterminada (''$KERNEL'') por la especificada aquí.
	-s <splash image>   especificar la imagen del arranque
	-o <output image>   especificar el nombre de la iso
	-v <volume name>    especificar el nombre del CD (es con el que se identifica el CD y que aparecerá en el nautilus al ser éste montado)
	-p <image prefix>   especificar el prefijo del nombre de la iso. El nombre por defecto es similar a "${IMAGEPREFIX}-$(date +%Y%m%d%H%M).iso" (cambiando la fecha y hora, naturalmente). Si especificaramos otro prefijo, como por ejemplo "test", el resultado sería un nombre del archivo iso tal que: "test-$(date +%Y%m%d%H%M).iso".

EOF

}

while getopts "xys:hv:o:p:m:" options
do
	case $options in
		h) usage
			exit 0
			;;
		x) NOSQUASH=1
			;;
		y) NOINITRAMFS=1
			;;
		s) SPLASH_IMAGE=$OPTARG
			;;
		v) VOLUMENAME=$OPTARG
			;;
		o) OUTPUTIMAGE=$OPTARG
			;;
		p) OUTPUTIMAGE=${ISOS}/${OPTARG}-$(date +%Y%m%d%H%M).iso
			;;
		m) MASTER=$OPTARG
			;;
		k) KERNEL=$OPTARG
			;;
		*) usage
			exit 1
			;;
	esac
done

if [ ! -d ${MASTER}/extras ]; then
	mkdir -p ${MASTER}/extras
fi

if [ ! -d ${MASTER}/isolinux ]; then
	mkdir -p ${MASTER}/isolinux
fi

if [ ! -d ${MASTER}/META ]; then
	mkdir -p ${MASTER}/META
fi

if [ $NOINITRAMFS ]; then
        echo "Skipping initramfs and isolinux..."
else
	cp -a /usr/lib/syslinux/isolinux.bin ${MASTER}/isolinux/
	cp -a /usr/share/genlive/isolinux/* ${MASTER}/isolinux/
# Copying extra contents to CD-ROM
	cp -a /usr/share/genlive/*.* ${MASTER}/
	cp -a /usr/share/genlive/extras/* ${MASTER}/extras
	if [ "$SPLASH_IMAGE" != "" ]
	then
		cp ${SPLASH_IMAGE} ${MASTER}/isolinux/splash.rle
	fi
	cp -a /boot/vmlinuz-${KERNEL} ${MASTER}/isolinux/vmlinuz
	mkinitramfs -o ${MASTER}/isolinux/initramfs ${KERNEL}
fi

if [ $NOSQUASH ] && [ -e ${MASTER}/META/META.squashfs ]; then
        echo "${MASTER}/META/META.squashfs already exist. Skipping mksquashfs..."
else        
        mksquashfs ${SOURCES} ${MASTER}/META/META.squashfs
fi

# Create a md5 checksum
# Exclude isolinux.bin cause it is created by mkisofs later
(cd ${MASTER} ; [ -f md5sum.lst ] && rm md5sum.lst ; for i in $(find . -type f | grep -v isolinux.bin); do md5sum $i >> md5sum.lst ; done)

#FIXME: Añadirle una opcion para cambiarle el isolinux.cfg

mkisofs -l -r -J -V "${VOLUMENAME}" -hide-rr-moved -v -b isolinux/isolinux.bin -c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -o  ${OUTPUTIMAGE} ${MASTER}
