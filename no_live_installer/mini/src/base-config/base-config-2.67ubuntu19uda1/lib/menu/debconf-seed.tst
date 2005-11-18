#!/bin/sh
set -e
[ -d /usr/share/base-config/debconf-seed ] || \
[ -e /var/log/installer/debconf-seed ] || \
[ -e /var/log/debian-installer/debconf-seed ]
