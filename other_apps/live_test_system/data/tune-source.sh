#!/bin/bash

echo "pushd / ..."
pushd /
echo "chown -R guada:guada /home/guada ..."
chown -R guada:guada /home/guada
echo "chown root:root etc/gksu.conf ..."
chown root:root etc/gksu.conf
echo "chown root:root var/run/sshd/ ..."
chown root:root var/run/sshd/
echo "apt-get update ..."
apt-get update
echo "Reconfiguring locales (please select es_ES.utf8) ..."
dpkg-reconfigure locales
echo "apt-get install -y openssh-server ..."
apt-get install -y openssh-server
echo "Deleting doc ..."
rm -rf usr/doc/* usr/share/doc/*
echo "Resetting language ..."
export LANGUAGE="es_ES.UTF-8"
echo "Removing old peez2, ubuntu-express and gparted packages ..."
dpkg --remove `dpkg -l | grep peez2 | cut -d' ' -f 3`
dpkg --remove `dpkg -l | grep ubuntu-express | cut -d' ' -f 3`
dpkg --remove `dpkg -l | grep gparted | cut -d' ' -f 3`
echo "Installing new peez2, ubuntu-express and gparted packages in tmp/ ..."
pushd tmp/
dpkg --install *.deb
apt-get -y install -f
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

