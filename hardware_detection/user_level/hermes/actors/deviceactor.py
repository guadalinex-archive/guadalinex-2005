# -*- coding: utf8 -*-

class DeviceActor(object):
    """
    Esta clase encapsula la lógica de actuación ante eventos en un dispositivo,

    Es una clase abstracta que sirve para crear "actuadores de dispositivos",
    que sirven para mostrar mensajes al insertar/quitar dispositivos
    """
    bus = None
    category = None

    def __init__(self, message_render, device_properties):
        self.message_render = self.msg_render = message_render
        self.properties = device_properties
        self.msg_no = None

    def on_added(self):
        raise NotImplementedError

    def on_removed(self):
        raise NotImplementedError


