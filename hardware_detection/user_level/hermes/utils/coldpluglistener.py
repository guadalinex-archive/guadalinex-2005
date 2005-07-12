#!/usr/bin/python
# -*- coding: utf8 -*-

import thread
import time

from utils import DeviceList

class ColdPlugListener:

    def __init__(self, devicelistener):
        self.devicelistener = devicelistener
        self.thread = None

    def start(self):
        lock = thread.allocate_lock()
        self.thread = thread.start_new_thread(self.__run, (lock, ))

    def __run(self, lock):
        lock.acquire()
        print "ColdPlugListener iniciado"
        dl = DeviceList()

        for ele in dl.get_added():
            try:
                self.devicelistener.on_device_added(ele[0]) #ele[0] contains the
                                                            #device udi
                time.sleep(2)
            except Exception, e:
                print e

        for ele in dl.get_removed():
            try:
                udi = ele[0]
                properties = ele[1]
                print "Procesando el eliminado: ", udi
                self.devicelistener.add_actor_from_properties(properties)
                self.devicelistener.on_device_removed(udi)
            except Exception, e:
                print e

        dl.save()
        lock.release()
