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


    def on_modified(self, key):
        print "Propiedad de volumeactor modificada"
        if key == 'volume.is_mounted':
            try:
                if properties['volume.is_mounted']:
                    self.message_render.show_info("Dispositivo montado en %s" %
                         (properties['volume.mount_point'],))
                else:
                    self.message_render.show_info("Dispositivo desmontado") 

            except:
                pass

