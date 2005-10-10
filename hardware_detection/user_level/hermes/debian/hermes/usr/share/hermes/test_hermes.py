#!/usr/bin/python
# -*- coding: utf8 -*-

import dbus
import unittest
import time 

class TrayInterfaceTest(unittest.TestCase):

    def setUp(self):
        bus = dbus.SessionBus()
        object = bus.get_object("org.guadalinex.TrayService", "/org/guadalinex/TrayObject")
        iface = dbus.Interface(object, "org.guadalinex.TrayInterface")

        self.iface = iface

    def test_show_message(self):
        self.iface.show_info("test_show_message")

        self.iface.show_warning("test_show_message")

        self.iface.show_error("test_show_message")


    def test_show_question(self):
        res = self.iface.show_question("test_show_question")
        self.assertEquals(res, 1)

        res_false = self.iface.show_question("test_show_question", 0)
        self.assertEquals(res_false, 0)

bus = dbus.SessionBus()
object = bus.get_object("org.guadalinex.TrayService", "/org/guadalinex/TrayObject")
iface = dbus.Interface(object, "org.guadalinex.TrayInterface")


def show_messages():
    iface.show_info("Mensaje de prueba")
    time.sleep(1)
    iface.show_warning("Mensaje de prueba")
    time.sleep(1)
    iface.show_error("Mensaje de prueba")

if __name__ == "__main__":
    unittest.main()
