#!/bin/bash

if [ 1 != $# ]; then
    echo "I need one argument: the path to the 'live_installer' branch of the repository!"
    exit 1
fi

if [ -e lab.OLD/ ]; then rm -rf lab.OLD/; fi
if [ -e lab/ ]; then mv lab/ lab.OLD/; fi
mkdir lab/
cp -R $1/default/ lab/
chmod u+x lab/default/debian/rules
pushd lab/default/lib/backend/
chmod a+x config.py copy.py format.py
popd
pushd lab/default
dpkg-buildpackage -rfakeroot
popd

if [ -e live_test/sources-breezy/tmp/ ]; then
    cp peez2*.deb lab/ubuntu-express*.deb live_test/sources-breezy/tmp/
fi

