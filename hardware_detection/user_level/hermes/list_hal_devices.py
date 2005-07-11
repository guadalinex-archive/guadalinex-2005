#!/usr/bin/python
# -*- coding: utf8 -*-

import dbus

bus = dbus.SystemBus()

obj = bus.get_object('org.freedesktop.Hal', '/org/freedesktop/Hal/Manager')
manager = dbus.Interface(obj, 'org.freedesktop.Hal.Manager')

for ele in manager.GetAllDevices():
    print ele

