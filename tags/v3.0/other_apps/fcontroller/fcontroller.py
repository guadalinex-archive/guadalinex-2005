#!/usr/bin/python
# -*- coding: utf-8 -*-

# Snapshot:
#
# $> fobserver.py -s dir snap_file
#
#
# Compare to snap_file:
#
# $> fcontroller.py dir snap_file
# AM   /etc/hosts
# AM   /etc/guadalinex-mini/
# D    /etc/guadalinex-mini/conf
# D    /etc/key.conf
# 

import sys
import os.path
import os
import gzip

from md5 import md5
from optparse import OptionParser
from sets import Set
from glob import glob

class FileObserver(object):

    def __init__(self, dirname, snap_filename):
        #Attributes
        if dirname[-1]=="/":
            dirname = dirname[:-1]

        self.dirname = dirname
        self.snap_filename = snap_filename


    def create_snap(self):
        """
        Create snap_file
        """

        snap_file = open(self.snap_filename, 'w')

        for ele in self.__get_snap_list():
            snap_file.write(ele[0])
            try:
                snap_file.write("    " + ele[1])
            except:
                pass

            snap_file.write("\n")
            
        snap_file.close()


    def diff(self):
        """
        Return a 2-tuple list:

        (op, filepath)

        """
        fs_set = Set([ele for ele in self.__get_snap_list()])
        result = []

        snap_set = Set()

        for line in open(self.snap_filename).readlines():
            split_ele = line.split(' ', 1)
            tuple_ele = (split_ele[0], split_ele[1].strip())
            
            snap_set.add(tuple_ele)

        #fs names
        fs_names_set = Set()
        for ele in fs_set:
            try:
                fs_names_set.add(ele[1])
            except:
                fs_names_set.add(ele[0])

        #snap names
        snap_names_set = Set()
        for ele in snap_set:
            try:
                snap_names_set.add(ele[1])
            except:
                snap_names_set.add(ele[0])

        #Added elements
        for ele in list(fs_names_set):
            snap_names_list = list(snap_names_set)
            if not ele in snap_names_list:
                result.append(("A", ele))

        #Modified elemnts 
        for ele in list(fs_set - snap_set):
            #If ele[1] (name) nos exists in snap_set, it's new.
            try:
                filedirpath = ele[1] 
            except:
                filedirpath = ele[0]
            
            if filedirpath in snap_names_list:
                result.append(("M", filedirpath))

        #Deleted elements
        for ele in list(snap_names_set):
            fs_names_list = list(fs_names_set)
            if not ele in fs_names_list:
                result.append(("D", ele))

        return result
            

    def __get_snap_list(self):
        snap_list = []
        for dirname, dirs, files in os.walk(self.dirname):
            yield ("directory", str(dirname))

            for file in files:
                file_path = dirname + os.sep + file
                try:
                    md5_object = md5(open(file_path).read())
                except:
                    continue
                md5_sum = str(md5_object.hexdigest())

                yield (md5_sum, file_path)
      

class PackageObserver:

    def __init__(self, fobserver):
        self.fobserver = fobserver
        #Search in the Contents cache (/var/cache/apt/*Contents-i386.gz)
        #self.file_cache = self.get_contents('/var/cache/apt/*Contents-i386.gz') 

    def diff(self):
        pack_dict = {}

        for ele in self.fobserver.diff():
            if ele[0] != "D":
                package_names = self.__get_package_names(ele[1])
                package_names = package_names or ["<unknow>"]

                for package in package_names:
                    if not pack_dict.has_key(package):
                        pack_dict[package] = Set() 

                    pack_dict[package].add(ele[1])

        return pack_dict


    def __get_package_names(self, filename):
        """
        Return the name of the filename's package container.
        """
        result = []
        f = os.popen('zgrep %s /var/cache/apt/*Contents-i386.gz 2>/dev/null' %
                filename[1:])

        for line in f.readlines():
            name = line.split(' ',1)[0].split(':')[-1]
            name = name.strip()

            if name == filename[1:]:
                line_split = line.rsplit(' ', 1)
                pkg_full_name_string = line_split[-1]
                
                for pkg_full_name in pkg_full_name_string.split(','):
                    pkg_full_name = pkg_full_name.strip()
                    pkg_name = pkg_full_name.split('/')[-1]

                    result.append(pkg_name)

        return result



def main():
    op = OptionParser(usage = '%prog dirname [-s|-p] snap_file')
    op.set_defaults(snapshot = False)
    op.set_defaults(packages = False)

    op.add_option('-s', '--snapshot',
            action = 'store_true',
            dest = 'snapshot',
            help = 'get a dir snapshot')

    op.add_option('-p', '--packages',
            action = 'store_true',
            dest = 'packages',
            help = "show file's package")

    (options, args) = op.parse_args()

    if options.packages and options.snapshot:
        op.error("only one option, please")

    if len(args) != 2:
        op.error("incorrect number of arguments")
        
    fo = FileObserver(args[0], args[1])
    if options.packages:
        po = PackageObserver(fo)
        for package_name, files in po.diff().items():
            print "[" + package_name + "]"
            for file in files:
                print "  ", file
            print 

    else:
        if options.snapshot:
            fo.create_snap()
        else:
            for ele in fo.diff():
                print ele[0],"  ", ele[1]


if __name__ == '__main__':
    main()
