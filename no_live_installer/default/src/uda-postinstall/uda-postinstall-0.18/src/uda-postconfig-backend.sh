#!/bin/bash

MP_PATH="/var/lib/molinux-postconfig/"

. $MP_PATH/post-config-info

#Adduser user-admin
deluser --remove-home molinux
addgroup --gid 1000 $USERNAME
CRYPT_PASS=$(echo "$PASSWD" | mkpasswd --hash=md5 --stdin)
useradd -m -u 1000 -s /bin/bash -g $USERNAME -p $CRYPT_PASS -G adm,dialout,cdrom,floppy,audio,dip,video,plugdev,lpadmin,scanner,admin $USERNAME

#Modify /etc/hosts
cat /etc/hosts | sed -e "s/molinux/$COMPUTER_NAME/g" > $MP_PATH/hosts
mv $MP_PATH/hosts /etc/hosts
chmod 644 /etc/hosts

#Modify /etc/hostname
echo "$COMPUTER_NAME" > /etc/hostname
hostname -F /etc/hostname

#Modify /etc/issue
echo "Molinux 1.2 (Dulcinea) \n \l" > /etc/issue

#Clean sources.list
cat /etc/apt/sources.list | grep -v "^#" | sed -e "/^$/d" > /etc/apt/sources.list.molinux


if [ "$MAIN_REPO" = "yes" ] ; 
then
	REPOS="main restricted"
fi

if [ "x$REPOS" != "x" ] ;
then
	echo "deb http://repositorios.molinux.info/molinux dulcinea $REPOS" >> /etc/apt/sources.list.molinux
else
	echo "#deb http://repositorios.molinux.info/molinux dulcinea main restricted" >> /etc/apt/sources.list.molinux
fi

if [ "$EXTRA_REPO" = "yes" ] ;
then
	echo "deb http://archive.ubuntu.com/ubuntu hoary universe" >> /etc/apt/sources.list.molinux
else
	echo "#deb http://archive.ubuntu.com/ubuntu hoary universe" >> /etc/apt/sources.list.molinux
fi

if [ "$SEC_REPO" = "yes" ] ;
then
	if [ "x$REPOS" != "x" ] ;
	then
		echo "deb http://repositorios.molinux.info/molinux dulcinea-seguridad $REPOS" \
		>> /etc/apt/sources.list.molinux
	else
		echo "#deb http://repositorios.molinux.info/molinux dulcinea-seguridad main restricted" \
		>> /etc/apt/sources.list.molinux
	fi
else
	echo "#deb http://repositorios.molinux.info/molinux dulcinea-seguridad main restricted" \
	>> /etc/apt/sources.list.molinux
fi


mv /etc/apt/sources.list.molinux /etc/apt/sources.list

