#!/bin/bash

if [ 1 != $# ]; then
    echo "I need one argument: the name of the source directory!"
    exit 1
fi

echo "cp tune-source.sh $1/tmp/ ..."
cp tune-source.sh $1/tmp/

echo "chmod u+x $1/tmp/tune-source.sh ..."
chmod u+x $1/tmp/tune-source.sh

echo "Now you should execute /tmp/tune-source.sh"
echo "NB: DON'T FORGET TO MOUNT AND UNMOUNT '/proc/'!"

echo "chroot $1 ..."
chroot $1

echo "rm $1/tmp/tune-source.sh ..."
rm $1/tmp/tune-source.sh

echo "chroot $1 ldconfig ..."
chroot $1 ldconfig

