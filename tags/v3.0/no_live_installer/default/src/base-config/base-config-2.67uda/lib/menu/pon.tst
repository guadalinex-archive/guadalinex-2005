#!/bin/sh
set -e

# s/390 has no ppp
machine=`uname -m`
if [ $machine = s390 ]; then
	exit 1
fi

if [ -x /usr/bin/pon ] && ( [ -e /etc/ppp/peers/provider ] || [ -x /usr/sbin/pppconfig ] ); then
	if ! route -n | grep -q '^0\.0\.0\.0'; then
		exit 0
	else
		exit 1
	fi
else
	exit 1
fi
