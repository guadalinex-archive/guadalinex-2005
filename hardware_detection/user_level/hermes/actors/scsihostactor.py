# -*- coding: utf8 -*-

from deviceactor import DeviceActor
  
class Actor(DeviceActor):
    category = 'scsi_host'

    def on_added(self):
        self.msg_render.show_info("Dispositivo scsi_host detectado")


    def on_removed(self):
        pass
