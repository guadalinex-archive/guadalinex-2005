#!/bin/bash

# This script check if each package contained at the directory passed as argument is updated.
# It's useful if you want to know the differents between the external repository and your 
# local repository

help () {
	echo 
	echo "Options:"
	echo "--pkg udeb|deb|all"
	echo "      udeb - Just operate with udebs"
	echo "      deb - Just operate with debs"
	echo "      all - Operate with all kind of packages, it's de default option"
}

check_sources_list () {
	echo "You should run 'apt-get update' before this program"
	BREEZY_LINES=$(cat /etc/apt/sources.list | grep breezy | grep ^deb)
	CHECK1=$(echo $BREEZY_LINES | grep " main ")
	CHECK2=$(echo $BREEZY_LINES | grep "main/debian-installer")
	CHECK3=$(cat /etc/apt/sources.list | grep breezy | grep ^deb-src)

	if [[ -z $CHECK1 || -z $CHECK2 || -z $CHECK3 ]]; then
		echo "WARNING: You should have in your sources.list:"
		echo "  deb http://archive.ubuntu.com/ubuntu breezy main universe"
		echo "  deb http://archive.ubuntu.com/ubuntu breezy main/debian-installer"
		echo "  deb-src http://uk.archive.ubuntu.com/ubuntu breezy main restricted"
	fi
}

[ -z "$1" ] && echo "Usage: $0 path-to-pool --options" && help && exit 1

cd $1
shift

UDEBS=$(find -name *.udeb)
DEBS=$(find -name *.deb)
PKGS=$UDEBS" "$DEBS
POOL=$1
UPGRADE=""

while [ $# -gt 0 ]; do
	case "$1" in 
		--pkg) 
			shift
			if [ "$1" == "udeb" ]; then
				PKGS=$UDEBS
			elif [ "$1" == "deb" ]; then
				PKGS=$DEBS
			elif [ "$1" == "all" ]; then
				echo ""
			else 
				echo "Unknow argument: $1, it should be udeb, deb or all"
				help
				exit 1
			fi
			shift
			;;
		--upgrade)
			UPGRADE=1
			;;
		*)
			echo "Unknow argument: $1"
			help
			exit 1
			;;
	esac
done

check_sources_list 

for PKG in $PKGS
do
	PKG_NAME=$(basename ${PKG/_*/})
	PKG_VER=$(dpkg -f $PKG Version)

	apt-cache show $PKG_NAME > /dev/null 2>&1

	if [ "$?" != "0" ]; then
		PKG_VER_EXT="0"
	else 
		PKG_VER_EXT=$(apt-cache show $PKG_NAME | grep ^Version | awk '{print $2}' | head -1)
	fi

	dpkg --compare-versions $PKG_VER_EXT \>\> $PKG_VER > /dev/null 2>&1
	if [ "$?" == "0" ]; then
		echo "There is a newer version for $PKG_NAME! new: $PKG_VER_EXT old: $PKG_VER"
	fi
done
