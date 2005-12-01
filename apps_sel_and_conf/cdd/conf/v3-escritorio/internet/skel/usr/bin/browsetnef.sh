#!/bin/sh

# wrapper script to unpackage winmail.dat

UNPACKTO=/tmp/tnef  # Warning: the contents of this directory will be deleted at the end of the script
TNEF=`which tnef`
FILEBROWSER=`which nautilus`
FILE=$1

if [[ ! -d $UNPACKTO ]];then
    mkdir $UNPACKTO
fi

$TNEF --file=$FILE --directory=$UNPACKTO --number-backups

$FILEBROWSER file:$UNPACKTO 
