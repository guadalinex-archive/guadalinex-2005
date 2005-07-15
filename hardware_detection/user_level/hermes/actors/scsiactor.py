# -*- coding: utf8 -*-
from deviceactor import DeviceActor

class Actor(DeviceActor):

    __required__ = {'info.bus': 'scsi'}

    def on_added(self):
        self.msg_render.show_info("Dispositivo SCSI conectado")


    def on_removed(self):
        pass


