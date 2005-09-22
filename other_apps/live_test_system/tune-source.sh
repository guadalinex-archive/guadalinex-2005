#!/bin/bash

echo "pushd / ..."
pushd /
echo "chown root:root etc/gksu.conf ..."
chown root:root etc/gksu.conf
echo "chown root:root var/run/sshd/ ..."
chown root:root var/run/sshd/
echo "Checking boot/grub/ ..."

if [ ! -e boot/grub/ ]; then
    mkdir boot/grub/
    cp lib/grub/i386-pc/stage1 lib/grub/i386-pc/stage2 lib/grub/i386-pc/e2fs_stage1_5 boot/grub/
fi

echo "apt-get update ..."
apt-get update
echo "dpkg -l | grep firefox-gnome-support || apt-get install -y firefox-gnome-support ..."
dpkg -l | grep firefox-gnome-support || apt-get install -y firefox-gnome-support
echo "Reconfiguring locales (please select es_ES.utf8) ..."
dpkg-reconfigure locales
echo "Deleting doc ..."
rm -rf usr/doc/* usr/share/doc/*
echo "Removing old peez2, ubuntu-express and gparted packages ..."
dpkg --remove `dpkg -l | grep peez2 | cut -d' ' -f 3`
dpkg --remove `dpkg -l | grep ubuntu-express | cut -d' ' -f 3`
dpkg --remove `dpkg -l | grep gparted | cut -d' ' -f 3`
echo "Installing new peez2, ubuntu-express and gparted packages in tmp/ ..."
pushd tmp/
dpkg --install *.deb
rm -rf *.deb
popd
echo "Cleaning APT cache ..."
apt-get clean
apt-get autoclean
echo "Making executable ubuntu-express backend scripts ..."
pushd usr/lib/python2.4/site-packages/ue/backend/
chmod a+x config.py copy.py format.py
popd
echo "Checking several directories ..."
if [ ! -e var/lib/gdm/ ]; then mkdir var/lib/gdm/; fi
if [ ! -e sys/ ]; then mkdir sys/; fi
if [ ! -e proc/ ]; then mkdir proc/; fi
if [ ! -e dev/ ]; then mkdir dev/; fi
if [ ! -e etc/sudoers ]; then echo "Warning: there is no etc/sudoers file!"; fi
echo "popd ..."
popd

