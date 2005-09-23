#!/bin/bash

MP_PATH="/var/lib/uda-postconfig/"

. $MP_PATH/post-config-info

#Adduser user-admin
# Uncomment the next line
deluser --remove-home uda
addgroup --gid 1000 $USERNAME
CRYPT_PASS=$(echo "$PASSWD" | mkpasswd --hash=md5 --stdin)
useradd -m -u 1000 -s /bin/bash -g $USERNAME -p $CRYPT_PASS -G adm,dialout,cdrom,floppy,audio,dip,video,plugdev,lpadmin,scanner,admin $USERNAME

#Modify /etc/hosts
ORIG=$(cat /etc/hosts | grep 127.0.0.1 | awk '{print $4}')
cat /etc/hosts | sed -e "s/$ORIG/$COMPUTER_NAME/g" > $MP_PATH/hosts
mv $MP_PATH/hosts /etc/hosts
chmod 644 /etc/hosts

#Modify /etc/hostname
echo "$COMPUTER_NAME" > /etc/hostname
hostname -F /etc/hostname

#Modify /etc/issue
echo "Guadalinex 2005 (Flamenco) \n \l" > /etc/issue

#Clean sources.list
cat /etc/apt/sources.list | grep -v "^#" | sed -e "/^$/d" > /etc/apt/sources.list.uda


if [ "$MAIN_REPO" = "yes" ] ; 
then
	REPOS="main restricted"
fi

if [ "x$REPOS" != "x" ] ;
then
	echo "deb http://archive.ubuntu.com/ubuntu breezy $REPOS" >> /etc/apt/sources.list.uda
else
	echo "#deb http://archive.ubuntu.com/ubuntu breezy main restricted" >> /etc/apt/sources.list.uda
fi

if [ "$EXTRA_REPO" = "yes" ] ;
then
	echo "deb http://archive.ubuntu.com/ubuntu breezy universe" >> /etc/apt/sources.list.uda
else
	echo "#deb http://archive.ubuntu.com/ubuntu breezy universe" >> /etc/apt/sources.list.uda
fi

if [ "$SEC_REPO" = "yes" ] ;
then
	if [ "x$REPOS" != "x" ] ;
	then
		echo "deb http://archive.ubuntu.com/ubuntu breezy-security $REPOS" \
		>> /etc/apt/sources.list.uda
	else
		echo "#deb http://archive.ubuntu.com/ubuntu breezy-security main restricted" \
		>> /etc/apt/sources.list.uda
	fi
else
	echo "#deb http://archive.ubuntu.com/ubuntu breezy-security main restricted" \
	>> /etc/apt/sources.list.uda
fi


mv /etc/apt/sources.list.uda /etc/apt/sources.list

