#-*- coding: utf8 -*-

from deviceactor import DeviceActor

class Actor (DeviceActor):
    category = "volume"

    def on_added(self):
        if self.properties['info.bus'] == 'usb':
            self.msg_render.show_info("Pendrive %s listo"
                 %(self.properties['volume.label']))

        else:
            self.msg_render.show_info("Dispositivo %s de volumen listo"
                %(self.properties['volume.label']))


    def on_removed(self):
        self.msg_render.show_info("Dispositivo %s de volumen desconectado" %
                (self.properties['volume.label']))
