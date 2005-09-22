#!/bin/bash

if [ 1 != $# ]; then
    echo "I need one argument: the name of the source directory!"
    exit 1
fi

echo cp tune-source.sh $1/tmp/ ...
cp tune-source.sh $1/tmp/
chmod u+x $1/tmp/tune-source.sh
echo now you should execute /tmp/tune-source.sh
chroot $1
rm $1/tmp/tune-source.sh
echo chroot $1 ldconfig ...
chroot $1 ldconfig

