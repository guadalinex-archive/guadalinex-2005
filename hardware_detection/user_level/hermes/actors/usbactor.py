# -*- coding: utf8 -*-

from deviceactor import DeviceActor

class Actor(DeviceActor):

    __required__ = {'info.bus' : 'usb'}


    def on_added(self):
        pass


    def  on_removed(self):
        pass

