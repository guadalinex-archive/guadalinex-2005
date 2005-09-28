#!/bin/bash

if [[ $# < 1 ]]; then
    echo "I need at least one argument: the path to the 'live_installer' branch of the repository!"
    exit 1
fi

if [[ $# > 2 ]]; then
    echo "Too many arguments!"
    exit 1
fi

if [ -e lab.OLD/ ]; then rm -rf lab.OLD/; fi
if [ -e lab/ ]; then mv lab/ lab.OLD/; fi
mkdir lab/
cp -R $1/default/ lab/
pushd lab/default/
dpkg-buildpackage -rfakeroot
popd

if [ 2 == $# ]; then

    if [ '-f' == $2 ]; then

	if [ -e lab.OLD/gparted/ ]; then
	    cp -R lab.OLD/gparted* lab/
	else
	    echo "Warning: there is no previous 'Gparted' version!"
	fi

    else
	echo "Warning: '$2' option not recognized!"
    fi

fi

if [ ! -e lab/gparted/ ]; then
    cp -R $1/gparted*cvs* lab/
    mv lab/gparted*cvs* lab/gparted/
    pushd lab/gparted/
    dpkg-buildpackage -rfakeroot
    popd
fi

if [ -e live_test/sources/tmp/ ]; then
    cp peez2*.deb lab/gparted*.deb lab/ubuntu-express*.deb live_test/sources/tmp/
fi

