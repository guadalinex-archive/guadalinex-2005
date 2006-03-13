#!/bin/sh
# Important: you need the package dpkg-dev installed in your system

[ "$1" == "" ] && echo "Usage:" && echo "download-sources.sh file_containing_packages_to_download" && exit

[ -d src ] || mkdir src

for pkg_src in $(cat $1)
do
	if [ "$pkg_src" == "linux-source-2.6.12" ]; then
		echo "Ignoring linux-source-2.6.12..."
	else
		[ -d src/$pkg_src ] || mkdir -p src/$pkg_src
		cd src/$pkg_src
		apt-get source $pkg_src
		cd ../..
	fi
done
