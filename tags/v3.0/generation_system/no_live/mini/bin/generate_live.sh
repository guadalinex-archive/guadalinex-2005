#! /bin/sh
set -e

[ -z "$CDIMAGE_ROOT" ] && echo "CDIMAGE_ROOT is not defined!!! You must do: export CDIMAGE_ROOT=/your_path_to_cdimage" && exit
. "$CDIMAGE_ROOT/etc/config"

IMAGE_TYPE="daily-live"
DATE="$(next-build-date "$IMAGE_TYPE")"
export CDIMAGE_LIVE=1
export COMPLETE=1
export PROJECT CAPPROJECT DIST ARCHES CDIMAGE_INSTALL

echo -n "Runnig run-germinate... "
#run-germinate > $CDIMAGE_ROOT/log/run-germinate.live.log
[ "$?" == "0" ] && echo  "OK"

echo -n "Running germinate-to-tasks... "
#germinate-to-tasks > $CDIMAGE_ROOT/log/germinate-to-tasks.live.log
[ "$?" == "0" ] && echo "OK"

echo -n "Running update-tasks... "
#update-tasks "$DATE" > $CDIMAGE_ROOT/log/update-tasks.live.log
[ "$?" == "0" ] && echo "OK"

echo -n "Running upgrader... "
#upgrader $CDIMAGE_ROOT > $CDIMAGE_ROOT/log/upgrader.live.log
[ "$?" == "0" ] && echo "OK"

echo "Running update-dist... "
#update-dist > $CDIMAGE_ROOT/log/update-dist.live.log
[ "$?" == "0" ] && echo "OK"

echo "Running debian-cd/build.sh..."
cd "$CDIMAGE_ROOT/debian-cd"
./build.sh

#mv ../scratch/guadalinex/debian-cd/i386/breezy-install-i386.raw ../scratch/guadalinex/debian-cd/i386/breezy-install-i386.iso

