#!/bin/bash

MP_PATH="/var/lib/uda-postconfig/"

. $MP_PATH/post-config-info

#Adduser user-admin
# Uncomment the next line
deluser --remove-home uda
addgroup --gid 1000 $USERNAME
CRYPT_PASS=$(echo "$PASSWD" | mkpasswd --hash=md5 --stdin)
#useradd -m -u 1000 -s /bin/bash -g $USERNAME -p $CRYPT_PASS -G adm,dialout,cdrom,floppy,audio,dip,video,plugdev,lpadmin,scanner,admin $USERNAME
useradd -m -u 1000 -s /bin/bash -g $USERNAME -p $CRYPT_PASS -G admin $USERNAME
/usr/local/sbin/adduser.local $USERNAME 1000 1000 /home/$USERNAME

chmod 755 /home/$USERNAME

#Modify /etc/hosts
#ORIG=$(cat /etc/hosts | grep 127.0.0.1 | awk '{print $4}')
#cat /etc/hosts | sed -e "s/$ORIG/$COMPUTER_NAME/g" > $MP_PATH/hosts
#mv $MP_PATH/hosts /etc/hosts
#chmod 644 /etc/hosts
echo "127.0.0.1 	localhost.localdomain	localhost	$COMPUTER_NAME" > /etc/hosts
echo "" >> /etc/hosts
echo "# The following lines are desirable for IPv6 capable hosts" >> /etc/hosts
echo "::1     ip6-localhost ip6-loopback" >> /etc/hosts
echo "fe00::0 ip6-localnet" >> /etc/hosts
echo "ff00::0 ip6-mcastprefix" >> /etc/hosts
echo "ff02::1 ip6-allnodes" >> /etc/hosts
echo "ff02::2 ip6-allrouters" >> /etc/hosts
echo "ff02::3 ip6-allhosts" >> /etc/hosts


#Modify /etc/hostname
echo "$COMPUTER_NAME" > /etc/hostname
hostname -F /etc/hostname

#Modify /etc/issue
#echo "Guadalinex v3 (Flamenco) \n \l" > /etc/issue

#Clean sources.list
cat /etc/apt/sources.list | grep -v "^#" | sed -e "/^$/d" > /etc/apt/sources.list.uda

echo '#TITLE:Sitio principal de Guadalinex en la Junta de Andalucia' >> /etc/apt/sources.list.uda
echo '#ID:jda' >> /etc/apt/sources.list.uda
echo "deb http://repositorio.guadalinex.org/ubuntu-breezy breezy main restricted universe multiverse" >> /etc/apt/sources.list.uda
echo "deb http://repositorio.guadalinex.org/guadalinex-flamenco flamenco main" >> /etc/apt/sources.list.uda
echo "deb http://repositorio.guadalinex.org/guadalinex-flamenco flamenco-updates main restricted universe multiverse" >> /etc/apt/sources.list.uda
echo "deb http://repositorio.guadalinex.org/guadalinex-flamenco flamenco-security main restricted universe multiverse" >> /etc/apt/sources.list.uda
echo "deb http://repositorio.guadalinex.org/guadalinex-flamenco flamenco-backports main restricted universe multiverse" >> /etc/apt/sources.list.uda
echo '#END' >> /etc/apt/sources.list.uda

mv /etc/apt/sources.list.uda /etc/apt/sources.list

# Move the correct lists into his place...
mv /usr/share/uda-postinstall/backend/repositorio* /var/lib/apt/lists

#Set up fstab...
# First, add some extra options to some partitions...
IFS=$'\n'
FSTAB_TEMP="/etc/fstab.temp"
for x in $(cat /etc/fstab)
do
        if [ -n "$(echo $x | grep media | grep -v cdrom | grep -v floppy)" ]; then
		if [ -z "$(echo $x | grep -e ntfs -e vfat)" ]; then
	                echo $x | sed -e s/defaults/defaults,users,exec,noauto/ >> $FSTAB_TEMP
		fi
        else
                echo $x >> $FSTAB_TEMP
        fi
done
mv $FSTAB_TEMP /etc/fstab

# And run update-grub :D

python /usr/share/uda-postinstall/backend/detect-additional-partitions.py
