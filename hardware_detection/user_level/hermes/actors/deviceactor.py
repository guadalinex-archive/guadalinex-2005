# -*- coding: utf8 -*-

"""
¿Cómo crear un Actor?

1) Creamos dentro del directorio actors un fichero .py, por ejemplo:
    actors/tvactor.py

2) Dentro de este fichero, lo primero que hacemos es importar la clase
DeviceActor dentro del módulo deviceactors:
        
   from deviceactor import DeviceActor

3) Creamos una clase que ha de llamarse Actor, y que hereda de DeviceActor:

    class Actor (DeviceActor):

4) Indicamos, a través del atributo de clase __required__ (de tipo dict), el nombre
y el valor de las propiedades requeridas para que el actor se active:

    __require__ = {'info.category' : 'tv'}


5) Redefinimos los métodos "on_added", "on_removed" y on_modified, con las acciones que se
deben realiza cuando se añada un dispositivo, se retire o se modifique una
propiedad del dispositivo para el que estamos actuando, respectivamente

Dentro de cualquier clase Actor disponemos de dos atributos sumamente útiles:

    self.msg_render: Es un objeto que implementa la interfaz
    org.guadalinex.TrayInterface, el cual usamos para comunicarnos con el
    usuario

    self.properties: Es un diccionario que contiene todas las propiedades del
    dispositivo sobre el que estamos actuando, proporcionado por HAL


El ejemplo completo:

------ actors/tvactor.py ------

from deviceactor import DeviceActor

class Actor(DeviceActor):
    __required__ ={'info.category': 'tv'}

    def on_added(self):
        self.msg_render.show_info("Dispositivo de tv detectado")

    def on_removed(self):
        self.msg_render.show_info("Dispositivo de tv desconectado")

    

"""
class DeviceActor(object):
    """
    Esta clase encapsula la lógica de actuación ante eventos en un dispositivo,

    Es una clase abstracta que sirve para crear "actores de dispositivos",
    que sirven para mostrar mensajes al insertar/quitar dispositivos
    """
    bus = None
    category = None
    __required__ = {} 

    def __init__(self, message_render, device_properties):
        self.message_render = self.msg_render = message_render
        self.properties = device_properties
        self.msg_no = None

    def on_added(self):
        pass

    def on_removed(self):
        pass
    
    def on_modified(self, prop_name):
        pass


