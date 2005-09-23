#!/bin/sh
set -e
. /usr/share/debconf/confmodule
db_get base-config/package-selection && [ "$RET" ]
