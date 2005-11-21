#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
sys.path.append('../')

import unittest
import dbus
import dbus.service
import thread
import threading
import gtk
import logging
import os
import os.path


logfilename = '/var/tmp/hermes-hardware-' + \
            os.environ['USER'] + str(os.getuid()) + \
            '.log' 

logging.basicConfig(level = logging.DEBUG,
           format='%(asctime)s %(levelname)s %(message)s',
           filename = logfilename,
           filemode='a')

DIR = os.path.dirname(__file__) + os.sep 


class HalDevice(dbus.service.Object):
    def __init__(self, bus_name, object_path, properties):
        dbus.service.Object.__init__(self, bus_name, object_path)
        self.properties = properties
        print "HalDevice:", properties

    @dbus.service.method("org.freedesktop.Hal.Device")
    def GetAllProperties(self):
        return self.properties



class VirtualHal(object):
    instance = None
    id = 0

    @staticmethod
    def get_instance():
        if not VirtualHal.instance:
            VirtualHal.instance = VirtualHal()
            gtk.threads_init()
            thread.start_new_thread(gtk.main, ())
        return VirtualHal.instance

    def __init__(self):
        name = dbus.service.BusName("org.freedesktop.Hal", 
                bus = dbus.SessionBus())
        modules = self.__scan_for_modules()
        self.devices = []
        for mod in modules:
            mod.udi = '/org/freedesktop/Hal/devices/test_' + \
                    str(VirtualHal.id)
            VirtualHal.id += 1
            mod.PROPERTIES['info.udi'] = mod.udi
            self.devices.append(HalDevice(name, mod.udi, mod.PROPERTIES))

    def __scan_for_modules(self):
        file_list = [ele for ele in os.listdir(DIR) if os.path.isfile(DIR + os.sep + ele)]
        dev_list = [ele for ele in file_list if (ele.startswith('dev_') and \
                                          ele.endswith('.py'))]
        modules_name = [filename.split('.')[0] for filename in dev_list]
        modules = [__import__(module_name, globals(), locals(),['*']) \
                    for module_name in modules_name]

        return modules

if __name__ == '__main__':
    vh = VirtualHal()
    gtk.main()
