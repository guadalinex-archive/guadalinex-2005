#!/bin/bash

PROJECT="Guadalinex"

[ -z "$1" ] && echo "Usage: $0 path-to-src-directory" && exit

find $1 -type f -iregex '.*\(po\|templates\)' -exec perl -pi -e s/Ubuntu/$PROJECT/g {} \;

#find $1 -name *templates* | grep -v templates.pot | grep -v S20templates | grep -v .c$ | grep -v .pl$ | grep -v po2templates | grep -v mktemplates | grep -v mk_shortlist_templates
