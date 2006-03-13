#!/bin/bash

[ -z "$1" ] && echo "Usage: $0 path-to-src-directory" && exit

ORIGIN=$(pwd)
cd $1
for DIR in $(ls -l | grep ^d | awk '{print $8}')
do
	VDIR=$(ls -l $DIR | grep ^d | awk '{print $8}')
	cd $DIR/$VDIR > /dev/null
	echo "BUILD DEPENDS:"
	echo "=============="
	cat debian/control | grep ^Build-Depends
	debuild -uc -us
	[ "$?" != "0" ] && echo "Problems compiling $DIR" && exit
	cd - > /dev/null
done
cd $ORIGIN 

#mv $(find -name *.udeb) $UDA_PKGS
