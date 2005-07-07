#!/usr/bin/python
# -*- coding: utf8 -*- 

import dbus
import gtk
from actors import CATEGORIES, BUSSES

class VolumeListener:
    
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

        #self.bus.add_signal_receiver(lambda *args: self.property_modified(udi, *args),
        #        dbus_interface = 'org.freedesktop.Hal.Device',
        #        signal_name = "PropertyModified")

        #self.bus.add_signal_receiver(self.on_device_removed,
        #                                "DeviceRemoved",       # member
        #                     'org.freedesktop.Hal.Manager',  # interface
        #                     'org.freedesktop.Hal',          # service
        #                     '/org/freedesktop/Hal/Manager') # path

        #self.udi_list contendrá una lista de los udi conectados en el sistema
        self.udi_list = []


    def on_device_added(self,  udi):
        self.bus.add_signal_receiver(lambda *args: self.property_modified(udi, *args),
                dbus_interface = 'org.freedesktop.Hal.Device',
                signal_name = "PropertyModified",
                path = udi)

        obj = self.bus.get_object('org.freedesktop.Hal', udi)
        obj = dbus.Interface(obj, 'org.freedesktop.Hal.Device')

        properties = obj.GetAllProperties()

        res1 = self.__process_bus(properties)
        res2 = self.__process_category(properties)

        if not (res1 or res2):
            self.message_render.show_info("Detectado el dispositivo %s" %
                    (properties['info.product'],))


    def __process_bus(self, prop):
        """
        return True if a bus device is detected. Otherwise return False 
        """
        print "Processing bus..."
        try: 
            bus = prop['info.bus']
            print "bus: ", bus
        except Exception:
            return False

        try:
            klass = BUSSES[bus]
            actor = klass(self.message_render, prop)
            actor.on_added()
            self.udi_list.append((prop['info.udi'], actor))
            return True
        except KeyError:
            pass


    def __process_category(self, prop):
        """
        Return True if a category devide is detected.Otherwise return False
        """
        print "Processing category..."
        try:
            category = prop['info.category']
            print "category: ", category
        except:
            return False

        try:
            klass = CATEGORIES[category]
            actor = klass(self.message_render, prop)
            actor.on_added()
            self.udi_list.append((prop['info.udi'], actor))
            return True
        except KeyError:
            pass
        
    
    def on_device_removed(self,  udi):
        self.message_render.abort()
        if udi in  [ele[0] for ele in self.udi_list]:
            ele[1].on_removed()
        else:
            self.message_render.show_warning("Dispositivo desconectado")


    def device_condition(self, condition_name, condition_details):
        print "condition_name: ", condition_name
        print "condition_details: ", condition_details
        if condition_name == 'VolumeMount':
            self.message_render.show_info("Dispositivo montado en ...")


    def property_modified(self, udi, num, values):
        print "Values: ", values
        for ele in values:
            key = ele[0]
            if key == 'volume.is_mounted':
                obj = self.bus.get_object('org.freedesktop.Hal', udi)
                obj = dbus.Interface(obj, 'org.freedesktop.Hal.Device')
                properties = obj.GetAllProperties()

                try:
                    if properties['volume.is_mounted']:
                        self.message_render.show_info("Dispositivo montado en %s" %
                             (properties['volume.mount_point'],))
                    else:
                        self.message_render.show_info("Dispositivo desmontado") 

                except:
                    pass

            




class DefaultMessageRender:

    def show_info (self, message):
        print "Default: ", message

    def abort(self):
        print "Mensaje abortado"


if __name__ == "__main__":
    bus = dbus.SessionBus()
    object = bus.get_object("org.guadalinex.TrayService", "/org/guadalinex/TrayObject")
    iface = dbus.Interface(object, "org.guadalinex.TrayInterface")

    VolumeListener(iface)
    gtk.main()


