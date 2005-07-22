#!/usr/bin/python
# -*- coding: utf8 -*- 

import dbus
import time
import logging
import gtk

from actors import ACTORSLIST
from utils import DeviceList, ColdPlugListener
from optparse import OptionParser


class DeviceListener:
    
    def __init__(self, message_render):
        self.message_render = message_render
        self.logger = logging.getLogger()

        #Inicialize
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
        self.modify_handler_dict = {}
        self.devicelist = DeviceList()

        self.__init_actors()

        coldplug = ColdPlugListener(self)
        coldplug.start()

        self.logger.info("DeviceListener iniciado")


    def on_device_added(self,  udi):
        self.logger.debug("DeviceAdded: " + str(udi))
        self.devicelist.save()

        obj = self.bus.get_object('org.freedesktop.Hal', udi)
        obj = dbus.Interface(obj, 'org.freedesktop.Hal.Device')

        properties = obj.GetAllProperties()
        self.__print_properties(properties)

        actor = self.add_actor_from_properties(properties)

        if actor: 
            actor.on_added()
        else: 
            if properties.has_key('info.vendor') and \
                    properties['info.vendor'] != '':
                product = properties['info.vendor']
                self.message_render.show_info("Dispositivo detectado: %s" %
                        (product,))
            else:
                self.message_render.show_warning("Dispositivo detectado, pero no identificado") 


    def on_device_removed(self,  udi): 
        self.logger.debug("DeviceRemoved: " + str(udi))
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
        klass = None
        actor_klass = None

        for klass in ACTORSLIST:
            count = self.__count_equals(prop, klass.__required__)
            if count > max:
                actor_klass = klass
                max = count

        actor = None 
        if actor_klass:
            udi = prop['info.udi']
            actor = actor_klass(self.message_render, prop)
            self.udi_dict[udi] = actor
            if not self.modify_handler_dict.has_key(udi):
                self.modify_handler_dict[udi] = lambda *args: self.on_property_modified(udi, *args) 
                self.bus.add_signal_receiver(self.modify_handler_dict[udi],
                    dbus_interface = 'org.freedesktop.Hal.Device',
                    signal_name = "PropertyModified",
                    path = udi)

        return actor


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


    def __init_actors(self):
        obj = self.bus.get_object('org.freedesktop.Hal', '/org/freedesktop/Hal/Manager')
        manager = dbus.Interface(obj, 'org.freedesktop.Hal.Manager')

        for udi in manager.GetAllDevices():
            obj = self.bus.get_object('org.freedesktop.Hal', udi)
            obj = dbus.Interface(obj, 'org.freedesktop.Hal.Device')

            properties = obj.GetAllProperties()
            self.add_actor_from_properties(properties)



def main():
    #Configure options
    parser = OptionParser(usage = 'usage: %prog [options]')
    parser.set_defaults(debug = False)

    parser.add_option('-d', '--debug', 
            action = 'store_true',
            dest = 'debug',
            help = 'start in debug mode')

    (options, args) = parser.parse_args()
    
    if options.debug:
        level = logging.DEBUG
    else:
        level = logging.INFO

    logging.basicConfig(level = level,
            format='%(asctime)s %(levelname)s %(message)s',
                    filename='/var/tmp/hermes-hardware.log',
                    filemode='a')

    #Connect to dbus
    bus = dbus.SessionBus()
    object = bus.get_object("org.guadalinex.Hermes", "/org/guadalinex/HermesObject")
    iface = dbus.Interface(object, "org.guadalinex.IHermesNotifier")

    DeviceListener(iface)
    gtk.main()


if __name__ == "__main__":
    main()


