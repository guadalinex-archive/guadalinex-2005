#!/bin/sh
set -e
[ ! -e /etc/console/boottime.kmap.gz ] && [ -f /bin/loadkeys ]
