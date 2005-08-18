#!/bin/bash
#This script will read a list of Debian package names from stdin and output the total size in bytes of those
#packages, dependencies included

#Get package list from standard input
packlist=`cat`

#Get list of dependencies of the packages in $packlist
#deplist=`apt-rdepends $packlist |grep '^[^ ]'|sed -e 's/^<\(.*\)>$/\1/'`
#deplist=`apt-cache depends --recurse --no-all-versions $packlist | grep "^  Depende"|awk '{ print $2 }'|sort|uniq|sed -e 's/^<\(.*\)>$/\1/'`
deplist=`apt-cache depends --recurse --no-all-versions $packlist | grep '^[^ ]'|sed -e 's/^<\(.*\)>$/\1/'`


#Merge both lists
mergedlist=`echo $packlist $deplist | sort | uniq`

#Get list of the sizes of every package, in bytes
sizes=`apt-cache show --no-all-versions $mergedlist | grep "^Size" | awk '{ print $2 }'`

#Sum all the sizes
sum=0
for i in $sizes; do
	sum=$(($sum+$i))
done

#Print the sum
echo $sum
