# -*- coding: utf8 -*-

from deviceactor import DeviceActor

class Actor(DeviceActor):

    __required__ = {'info.bus' : 'usb_device'}

    def on_added(self):
        try:
            product = self.properties['usb_device.product']
            vendor = self.properties['info.vendor']
            vendor = vendor and vendor + ', ' or ''
            self.msg_render.show_info("Dispositivo usb detectado: " + vendor +\
                    product)
        except:
            self.msg_render.show_info("Dispositivo usb detectado")


    def  on_removed(self):
        self.msg_render.show_info("Dispositivo usb desconectado")
