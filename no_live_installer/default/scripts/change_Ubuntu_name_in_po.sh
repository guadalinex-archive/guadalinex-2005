#!/bin/bash

PROJECT="Guadalinex"

[ -z "$1" ] && echo "Usage: $0 path-to-src-directory" && exit

cd $1

for DIR in $(ls -l | grep ^d | awk '{print $8}')
do
	VDIR=$(ls -l $DIR | grep ^d | awk '{print $8}')
	if [ -f $DIR/$VDIR/debian/po/es.po ]; then

		cat $DIR/$VDIR/debian/po/es.po | sed s/Ubuntu/$PROJECT/g > $DIR/$VDIR/debian/po/es.po

		if [ "$DIR" == "glibc" ]; then
			TEMPLATES="$DIR/$VDIR/debian/debhelper.in/locales.templates"
		else
			TEMPLATES=$(ls $DIR/$VDIR/debian/*templates*)
		fi

		if [ "$TEMPLATES" != "" ]; then
			for TEMPLATE in $TEMPLATES
			do
				cat $TEMPLATE | sed s/Ubuntu/$PROJECT/g > $TEMPLATE
			done
		fi
	else
		echo "Dir without es.po: "$DIR
	fi
done
cd -
