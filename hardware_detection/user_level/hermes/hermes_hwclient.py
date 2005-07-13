#!/usr/bin/python
# -*- coding: utf8 -*- 

import dbus
import gtk

from actors import CATEGORIES, BUSSES
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

        actor1 = self.__process_bus(properties)
        actor2 = self.__process_category(properties)

        if actor1: actor1.on_added()
        if actor2: actor2.on_added()

        if not (actor1 or actor2):
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
        This method is useful for add device information on removed
        """
        actor1 = self.__process_bus(prop)
        actor2 = self.__process_category(prop)

        if not (actor1 or actor2):
            return False
        else:
            return True


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

    def __process_bus(self, prop):
        """
        return True if a bus device is detected. Otherwise return False 
        """
        try: 
            bus = prop['info.bus']
        except Exception:
            return None

        try:
            klass = BUSSES[bus]
            actor = klass(self.message_render, prop)
            #actor.on_added()
            self.udi_dict[prop['info.udi']] = actor
            return actor
        except KeyError:
            return None


    def __process_category(self, prop):
        """
        Return True if a category devide is detected. Otherwise return False
        """
        try:
            category = prop['info.category']
        except:
            return None

        try:
            klass = CATEGORIES[category]
            actor = klass(self.message_render, prop)

            #try:
            #    actor.on_added()
            #except:
            #    pass

            self.udi_dict[prop['info.udi']] = actor
            return actor
        except KeyError, e:
            return None
        
    
    def device_condition(self, condition_name, condition_details):
        if condition_name == 'VolumeMount':
            self.message_render.show_info("Dispositivo montado en ...")


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


