#!/usr/bin/python
# -*- coding: utf8 -*- 

import dbus
import gtk

from actors import ACTORSLIST
from utils import DeviceList, ColdPlugListener


class DeviceListener:
    
    def __init__(self, message_render):
        self.message_render = message_render

        self.bus = dbus.Bus(dbus.Bus.TYPE_SYSTEM)
        obj = self.bus.get_object('org.freedesktop.Hal',
                                  '/org/freedesktop/Hal/Manager')

        self.hal_manager = dbus.Interface(obj, 'org.freedesktop.Hal.Manager')

        self.bus.add_signal_receiver(self.on_device_added, 
                                 dbus_interface = 'org.freedesktop.Hal.Manager',
                                 signal_name = 'DeviceAdded')

        self.bus.add_signal_receiver(self.on_device_removed, 
                                 dbus_interface = 'org.freedesktop.Hal.Manager',
                                 signal_name = 'DeviceRemoved')

        self.udi_dict = {}

        self.devicelist = DeviceList()
        coldplug = ColdPlugListener(self)
        coldplug.start()

        print "DeviceListener iniciado"


    def on_device_added(self,  udi):
        self.devicelist.save()

        self.bus.add_signal_receiver(lambda *args: self.on_property_modified(udi, *args),
                dbus_interface = 'org.freedesktop.Hal.Device',
                signal_name = "PropertyModified",
                path = udi)

        obj = self.bus.get_object('org.freedesktop.Hal', udi)
        obj = dbus.Interface(obj, 'org.freedesktop.Hal.Device')

        properties = obj.GetAllProperties()
        self.__print_properties(properties)

        actor = self.add_actor_from_properties(properties)

        if actor: 
            actor.on_added()
        else: 
            try:
                product = properties['info.vendor']
                self.message_render.show_info("Dispositivo detectado: %s" %
                        (product,))
            except:
                self.message_render.show_warning("Dispositivo detectado, pero no identificado") 


    def on_device_removed(self,  udi): 
        self.devicelist.save()

        if self.udi_dict.has_key(udi):
            disp = self.udi_dict[udi]
            disp.on_removed()
            print
            print
            print "#############################################"
            print "DESCONEXIÓN  ################################"
            print "#############################################"
            self.__print_properties(disp.properties)
            del self.udi_dict[udi]
        else:
            self.message_render.show_warning("Dispositivo desconectado")


    def on_property_modified(self, udi, num, values):
        for ele in values:
            key = ele[0]

            if udi in self.udi_dict.keys():
                #Actualizamos las propiedades del objeto actor
                actor = self.udi_dict[udi]
                obj = self.bus.get_object('org.freedesktop.Hal', udi)
                obj = dbus.Interface(obj, 'org.freedesktop.Hal.Device')

                actor.properties = obj.GetAllProperties()
                actor.on_modified(key)


    def add_actor_from_properties(self, prop):
        """
        Devuelve un actor que pueda actuar para dispositivos con las propiedades
        espeficicadas en prop
        """
        max = 0
        actor = None

        for klass in ACTORSLIST:
            count = self.__count_equals(prop, klass.__required__)
            if count > max:
                actor = klass(self.message_render, prop)
                max = count
        
        if actor:
            self.udi_dict[prop['info.udi']] = actor

        return actor


    def device_condition(self, condition_name, condition_details):
        if condition_name == 'VolumeMount':
            self.message_render.show_info("Dispositivo montado en ...")


    def __print_properties(self, properties):
        print 
        print 
        print '-----------------------------------------------'
        print "Dispositivo: ", properties['info.udi']
        print 
        keys = properties.keys()
        keys.sort()

        for key in keys:
            print key + ':' + str(properties[key])

    def __count_equals(self, prop, required):
        """
        Devuelve el número de coincidencias entre el diccionario prop y
        required, siempre y cuando TODOS los elementos de required estén en
        prop.
        En caso contrario devuelve 0.
        """
        count = 0
        for key in required.keys():
            if not prop.has_key(key): 
                return 0

            if prop[key] != required[key]:
                return 0
            count += 1

        return  count



class DefaultMessageRender:

    def show_info (self, message):
        print "Default: ", message

    def abort(self):
        print "Mensaje abortado"


if __name__ == "__main__":
    bus = dbus.SessionBus()
    object = bus.get_object("org.guadalinex.TrayService", "/org/guadalinex/TrayObject")
    iface = dbus.Interface(object, "org.guadalinex.TrayInterface")

    gtk.gdk.threads_init()
    DeviceListener(iface)
    gtk.main()


