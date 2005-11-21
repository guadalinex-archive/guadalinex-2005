#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
sys.path.append('../')

import unittest
import hermes_hardware
import dbus
import logging
import os
import os.path
import time

from utils.notification import NotificationDaemon
from virtualhal import VirtualHal


logfilename = '/var/tmp/hermes-hardware-' + \
            os.environ['USER'] + str(os.getuid()) + \
            '.log' 

logging.basicConfig(level = logging.DEBUG,
           format='%(asctime)s %(levelname)s %(message)s',
           filename = logfilename,
           filemode='a')

        
class ActorTest(unittest.TestCase):
    "Base class for actor tests"

    devicelistener = None

    def setUp(self):
        if not ActorTest.devicelistener:
            ActorTest.devicelistener = hermes_hardware.DeviceListener(NotificationDaemon())
            ActorTest.devicelistener.bus = dbus.SessionBus()
            ActorTest.virtualhal = VirtualHal()

    
    
if __name__ == '__main__':
    vh = VirtualHal()
    gtk.main()
