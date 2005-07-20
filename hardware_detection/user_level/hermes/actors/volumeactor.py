#-*- coding: utf8 -*-

from deviceactor import DeviceActor

class Actor (DeviceActor):

    __required__ = {'info.category': 'volume'}

    #def on_added(self):
    #    self.msg_render.show_info("Dispositivo de volumen conectado")

    def on_modified(self, key):
        if key == 'volume.is_mounted':
            try:
                if self.properties['volume.is_mounted']:
                    self.message_render.show_info("Dispositivo montado en %s" %
                         (self.properties['volume.mount_point'],))
                else:
                    self.message_render.show_info("Dispositivo desmontado") 

            except Exception, e:
                print "Error: ", e

