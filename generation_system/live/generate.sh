#!/bin/bash
#

SOURCES="/media/distro/sources"
MASTER="/media/distro/master"
LOG_FILE="/var/log/installer.log"

# Loading configuration
. ./ue.conf

# Initial processes
init_processes () {
	# Cleaning $MASTER directory
	[ ! -e $MASTER ] && mkdir -p $MASTER
        find $MASTER -maxdepth 1 -exec rm -rf {} \;
        cp -r $CALZADORES_LATEST $MASTER || exit 1
        mkdir $MASTER/META
}

# Generate sources if they doesn't exist
debootstrap_start () {
	if [ ! -e $SOURCES/lib/ ]; then
	  [ ! -e $SOURCES ] && mkdir -p $SOURCES;
	  chroot $SOURCES debootstrap --include="$(cat ./packages)" breezy $SOURCES ftp://archive.ubuntu.com/ubuntu/
	fi
}

# Cleaning garbage from developing tasks
chroot_clean () {
  echo "[$(date +%Y%m%d%H%M)] Starting process..." >> $LOG_FILE
  echo "[$(date +%Y%m%d%H%M)] Cleaning  image..." >> $LOG_FILE
# FIXME:/home/usuario ?
  chroot $SOURCES find /tmp /home/usuario /root -maxdepth 1 -exec rm -rf {} \;
  > $SOURCES/etc/resolv.conf
}

# Tag distro images with date and warning message.
tag_images () {
	echo "[$(date +%Y%m%d%H%M)] Tagging graphics..." >> $LOG_FILE

	# Tagging background
	if [ $VERSION_TYPE == 'DEVEL' ]; then
		convert -fill white -font helvetica -pointsize $POINTSIZETEXTOS \
		  -draw "gravity center text 0,0 \"$GRAPH_TAG_WARNING_TEXT\"" \
		  -pointsize $POINTSIZECODIGOS \
		  -draw "gravity northwest text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
		  -draw "gravity southeast text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
		  $GRAPHICS_SOURCE_DIR/latest/guadalinex-background.png \
		  $SOURCES/usr/share/pixmaps/guadalinex/guadalinex-background.png
	else 
		cp $GRAPHICS_SOURCE_DIR/latest/guadalinex-background.png \
		  $SOURCES/usr/share/pixmaps/guadalinex/guadalinex-background.png 
	fi || exit 1

	# Tagging gnome splash screen
	if [ $VERSION_TYPE == 'DEVEL' ]; then
	convert -fill white -font helvetica \
	  -pointsize $POINTSIZECODIGOS \
	  -draw "gravity southeast text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
	  $GRAPHICS_SOURCE_DIR/latest/guadalinex-splash.png \
	  $SOURCES/usr/share/pixmaps/guadalinex/guadalinex-splash.png
	else
	  cp $GRAPHICS_SOURCE_DIR/latest/guadalinex-splash.png \
	  $SOURCES/usr/share/pixmaps/guadalinex/guadalinex-splash.png
	fi

	# Tagging gdm background
	if [ $VERSION_TYPE == 'DEVEL' ]; then
	convert -fill white -font helvetica -pointsize $POINTSIZETEXTOS \
	  -draw "gravity center text 0,0 \"$GRAPH_TAG_WARNING_TEXT\"" \
	  -pointsize $POINTSIZECODIGOS \
	  -draw "gravity northwest text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
	  -draw "gravity southeast text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
	  $GRAPHICS_SOURCE_DIR/latest/background.jpg \
	  $SOURCES/usr/share/gdm/themes/guadalinex/background.jpg
	else
	  cp $GRAPHICS_SOURCE_DIR/latest/background.jpg \
             $SOURCES/usr/share/gdm/themes/guadalinex/background.jpg
	fi

	# Tagging bootsplash images
	if [ $VERSION_TYPE == 'DEVEL' ]; then
	convert -fill white -font helvetica -pointsize $POINTSIZETEXTOS \
	  -draw "gravity center text 0,0 \"$GRAPH_TAG_WARNING_TEXT\"" \
	  -pointsize $POINTSIZECODIGOS \
	  -draw "gravity northwest text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
	  -draw "gravity southeast text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
	  -quality $GRAPHICS_QUALITY \
	  $GRAPHICS_SOURCE_DIR/latest/silent-1024x768.jpg \
	  $SOURCES/usr/share/bootsplash/themes/Linux/images/silent-1024x768.jpg
	else
	  cp $GRAPHICS_SOURCE_DIR/latest/silent-1024x768.jpg \
	  $SOURCES/usr/share/bootsplash/themes/Linux/images/silent-1024x768.jpg
	fi

	if [ $VERSION_TYPE == 'DEVEL' ]; then
	convert -fill white -font helvetica -pointsize $POINTSIZETEXTOS \
	  -draw "gravity center text 0,0 \"$GRAPH_TAG_WARNING_TEXT\"" \
	  -pointsize $POINTSIZECODIGOS \
	  -draw "gravity northwest text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
	  -draw "gravity southeast text 70,70 \"$GRAPH_TAG_SERIAL_TEXT\"" \
	  -quality $GRAPHICS_QUALITY \
	  $GRAPHICS_SOURCE_DIR/latest/bootsplash-1024x768.jpg \
	  $SOURCES/usr/share/bootsplash/themes/Linux/images/bootsplash-1024x768.jpg
	else
	  cp $GRAPHICS_SOURCE_DIR/latest/bootsplash-1024x768.jpg \
	  $SOURCES/usr/share/bootsplash/themes/Linux/images/bootsplash-1024x768.jpg
	fi

	# More bootsplash stuff
	splash -s -f $GRAPHICS_SOURCE_DIR/bootsplash.cfg > $SOURCES/boot/initrd.splash
	splash -s -f $GRAPHICS_SOURCE_DIR/bootsplash.cfg >> $MASTER/isolinux/initrd

	# Copying calzador
	cp -f $CALZADORES_LATEST/initrd $MASTER/isolinux/initrd

}

tag_texts () {
	# Tagging distro from actual date
	echo "[$(date +%Y%m%d%H%M)] Tagging text files..." >> $LOG_FILE

	# Creating version files
	echo "$fecha_actual\n$VERSION_TAG\n$HOSTNAME" > ${SOURCES}/etc/guadalinex_version
	echo "$fecha_actual\n$VERSION_TAG\n$HOSTNAME" > ${MASTER}/version

	# Modifying isolinux screen
	sed -e "s/FECHA/$fecha_actual/g" -e "s/VTAG/$VERSION_TAG/g" $CALZADORES_LATEST/greeting > ${MASTER}/isolinux/greeting
}

# Adding extra stuff into the CD.
add_extra_stuff () {
  	echo "[$(date +%Y%m%d%H%M)] Adding extra files..." >> $LOG_FILE

  	cp -r $EXTRA_DIR/* $MASTER || exit 1
}

# Create distro iso image.
create_image () {
	# Delete old image
	rm -f $MASTER/META/META.{squash,cloop}

	case $TIPO_IMAGEN in
		cloop)
			create_cloop
			;;
		squashfs)
			create_squashfs
			;;
	esac
}

create_squashfs () {
	echo "[$(date +%Y%m%d%H%M)] Creating META.squash image..." >> $LOG_FILE

	mksquashfs2 $SOURCES $MASTER/META/META.squash
}

create_cloop () {
	# Creating an ISO image from distro
	echo "[$(date +%Y%m%d%H%M)] Creating image META.cloop..." >> $LOG_FILE

	mkisofs -R -L -allow-multidot -l -V "${VOLNAME} iso9660 filesystem" \
	-o $ISODIR/${META}_.iso -hide-rr-moved -v $SOURCES >>$LOG_FILE \
	2>>$LOG_FILE || exit 1

	# Compressing image
	echo "[$(date +%Y%m%d%H%M)] Creating META.cloop..." >> $LOG_FILE

	create_compressed_fs $ISODIR/${META}_.iso 65536 > $MASTER/META/META.cloop 2>>$LOG_FILE || exit 1
}

generate_md5sum () {
	cd $MASTER
	rm -f md5s md5s.tmp

	for FILE in $(find . -type f); do
		md5sum $FILE >> md5s.tmp || exit 1
	done
	cat md5s.tmp | grep -v "isolinux.bin" | grep -v "boot.cat" | sort > md5s
	rm -f md5s.tmp
}

# Creating final compressed iso image
create_final_iso () {
	echo "[$(date +%Y%m%d%H%M)] Creating final compressed image..." >> $LOG_FILE

	mkisofs -l -r -J -V "${VOLNAME}" -hide-rr-moved -v -b isolinux/isolinux.bin -c isolinux/boot.cat \
		-no-emul-boot -boot-load-size 4 -boot-info-table -o $ISODIR/${META}.iso $MASTER \
		>>$LOG_FILE 2>$LOG_FILE || exit 1
}

# Create a link to last generated image
create_link_latest () {
	rm -f ${ISODIR}/${LINK_NAME}
	ln -s ${ISODIR}/${META}.iso ${ISODIR}/${LINK_NAME}
}

# Final processes
final_processes () {
  echo "[$(date +%Y%m%d%H%M)] Final processes..." >> $LOG_FILE

  # Delete $MASTER content
  find $MASTER/ -maxdepth 1 -exec rm -rf {} \;
}

main () {
	init_processes
	debootstrap_start
	chroot_clean
	tag_images
	tag_texts
	add_extra_stuff
	create_image
	generate_md5sum
	create_final_iso
	create_link_latest
	final_processes
}

main
