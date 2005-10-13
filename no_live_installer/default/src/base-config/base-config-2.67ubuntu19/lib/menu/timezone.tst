#!/bin/sh -e

[ "$1" = new ] || exit 0

if [ -f /etc/timezone ]; then
	TIMEZONE="$(cat /etc/timezone)"
elif [ -L /etc/localtime ]; then
	TIMEZONE="$(readlink /etc/localtime | sed 's%^/usr/share/zoneinfo/%%')"
else
	TIMEZONE=
fi

if [ "$TIMEZONE" ] && [ "$TIMEZONE" != UTC ]; then
	# Timezone already set up in first stage (assume GMT hardware clock
	# was too), so skip this step.
	exit 1
else
	exit 0
fi
