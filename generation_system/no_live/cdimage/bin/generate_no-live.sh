#! /bin/sh
set -e

[ -z "$CDIMAGE_ROOT" ] && export CDIMAGE_ROOT=/home/carlospc/developing/cdimage
. "$CDIMAGE_ROOT/etc/config"

IMAGE_TYPE="${1:-daily}"
DATE="$(next-build-date "$IMAGE_TYPE")"
CDIMAGE_INSTALL=1

export PROJECT CAPPROJECT DIST ARCHES CDIMAGE_INSTALL

echo -n "Runnig run-germinate... "
run-germinate > $CDIMAGE_ROOT/log/run-germinate.log
[ "$?" == "0" ] && echo  "OK"

echo -n "Running germinate-to-tasks... "
germinate-to-tasks > $CDIMAGE_ROOT/log/germinate-to-tasks.log
[ "$?" == "0" ] && echo "OK"

echo -n "Running update-tasks... "
update-tasks "$DATE" > $CDIMAGE_ROOT/log/update-tasks.log
[ "$?" == "0" ] && echo "OK"

echo -n "Running update-dist... "
update-dist > $CDIMAGE_ROOT/log/update-dist.log
[ "$?" == "0" ] && echo "OK"

echo "Running debian-cd/build_all.sh..."
cd "$CDIMAGE_ROOT/debian-cd"
./build.sh
