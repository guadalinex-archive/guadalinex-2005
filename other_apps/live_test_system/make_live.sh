#!/bin/bash

SOURCESDIR=/sources
ISONAME=gx2005.iso
NAME="Custom Gx2005 Live"

EXTRACTED_CD=./extracted_cd
#Creamos la imagen
echo Comprimiendo el sistema fuente...
mkdir mnt.new
sudo dd if=/dev/zero of=extracted_fs.new bs=1M count=2000
yes s | sudo mkfs.ext2 extracted_fs.new
sudo mount -o loop,noatime,nodiratime extracted_fs.new mnt.new
sudo rsync -avx --progress $SOURCESDIR/. mnt.new/.
sudo umount mnt.new 
rmdir mnt.new
sudo create_compressed_fs extracted_fs.new 65536 > $EXTRACTED_CD/casper/filesystem.cloop
sudo rm extracted_fs.new

#Re-generamos el fichero .manifest
#sudo chroot $SOURCESDIR dpkg-query -W --showformat='${Package} ${Version}\n' > $EXTRACTED_CD/casper/filesystem.manifest

#Sumas md5
echo Creando el md5sum.txt...
(cd $EXTRACTED_CD && find . -type f -print0 | xargs -0 md5sum > md5sum.txt)

#Creamos la iso
echo Creando la iso...
sudo mkisofs -r -V "$NAME" \
            -cache-inodes \
            -J -l -b isolinux/isolinux.bin \
            -c isolinux/boot.cat -no-emul-boot \
            -boot-load-size 4 -boot-info-table \
            -o $ISONAME $EXTRACTED_CD

