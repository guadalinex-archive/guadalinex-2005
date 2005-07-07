# -*- coding: utf8 -*-

from deviceactor import DeviceActor

class Actor(DeviceActor):
    bus = 'usb_device'

    def on_added(self):
        self.msg_render.show_info("Dispositivo usb conectado")


    def  on_removed(self):
        self.msg_render.show_info("Dispositivo usb desconectado")
