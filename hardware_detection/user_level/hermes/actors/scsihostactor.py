# -*- coding: utf8 -*-

from deviceactor import DeviceActor
  
class Actor(DeviceActor):

    __required__ = {'info.category' : 'scsi_host'}

    def on_added(self):
        vendor = ''

        if self.properties.has_key('info.vendor'):
            vendor = ': ' + self.properties['info.vendor']

        self.msg_render.show_info("Dispositivo scsi_host detectado" + vendor )


    def on_removed(self):
        pass
