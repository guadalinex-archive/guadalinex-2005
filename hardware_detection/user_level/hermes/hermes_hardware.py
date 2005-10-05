#!/usr/bin/python
# -*- coding: utf8 -*- 

#Módulo hermes_hardware - Notificador de cambios en el hardware
#
#Copyright (C) 2005 Junta de Andalucía
#
#Autor/es (Author/s):
#
#- Gumersindo Coronel Pérez <gcoronel@emergya.info>
#
#Este fichero es parte de Detección de Hardware de Guadalinex 2005 
#
#Detección de Hardware de Guadalinex 2005  es software libre. Puede redistribuirlo y/o modificarlo 
#bajo los términos de la Licencia Pública General de GNU según es 
#publicada por la Free Software Foundation, bien de la versión 2 de dicha
#Licencia o bien (según su elección) de cualquier versión posterior. 
#
#Detección de Hardware de Guadalinex 2005  se distribuye con la esperanza de que sea útil, 
#pero SIN NINGUNA GARANTÍA, incluso sin la garantía MERCANTIL 
#implícita o sin garantizar la CONVENIENCIA PARA UN PROPÓSITO 
#PARTICULAR. Véase la Licencia Pública General de GNU para más detalles. 
#
#Debería haber recibido una copia de la Licencia Pública General 
#junto con Detección de Hardware de Guadalinex 2005 . Si no ha sido así, escriba a la Free Software
#Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
#
#-------------------------------------------------------------------------
#
#This file is part of Detección de Hardware de Guadalinex 2005 .
#
#Detección de Hardware de Guadalinex 2005  is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation; either version 2 of the License, or
#at your option) any later version.
#
#Detección de Hardware de Guadalinex 2005  is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with Foobar; if not, write to the Free Software
#Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import dbus
if getattr(dbus, "version", (0, 0, 0)) >= (0, 41, 0):
    import dbus.glib
import logging
import gtk

from actors import ACTORSLIST
from utils import DeviceList, ColdPlugListener
from optparse import OptionParser


        #notification-daemon spec: -------------------------------------------
        #http://galago.info/specs/notification/0.7/index.html

        #  UINT32 org.freedesktop.Notifications.Notify 
        #  (STRING app_name, BYTE_ARRAY_OR_STRING app_icon, UINT32 replaces_id, 
        #  STRING notification_type, BYTE urgency_level, STRING summary, 
        #  STRING body, ARRAY images, DICT actions, DICT hints, BOOL expires, 
        #  UINT32 expire_timeout);
        
        #self.iface.Notify("Hermes", #app_name 
        #        '', # app_icon
        #        0, # replaces_id
        #        'device.added', # notification_type
        #        1, # urgency_level
        #        '', # summary
        #        message, # body
        #        '', # images
        #        '', # actions
        #        '', # hints
        #        True, # expires
        #        0  #expire_timeout
        #        )


class DeviceListener:
    
    def __init__(self, message_render):
        self.message_render = message_render
        self.logger = logging.getLogger()

        #Inicialize
        self.bus = dbus.SystemBus()

        obj = self.bus.get_object('org.freedesktop.Hal',
                                  '/org/freedesktop/Hal/Manager')

        self.hal_manager = dbus.Interface(obj, 'org.freedesktop.Hal.Manager')

        self.hal_manager.connect_to_signal('DeviceAdded', self.on_device_added)
        self.hal_manager.connect_to_signal('DeviceRemoved', 
                self.on_device_removed)

        self.udi_dict = {}
        self.modify_handler_dict = {}
        self.devicelist = DeviceList()

        self.__init_actors()

        coldplug = ColdPlugListener(self)
        coldplug.start()

        self.logger.info("DeviceListener iniciado")


    def on_device_added(self, udi, *args):
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
                self.message_render.show_info("Información", "Dispositivo detectado: %s" %
                        (product,))
            else:
                self.message_render.show_warning("Aviso", "Dispositivo detectado, pero no identificado") 


    def on_device_removed(self, udi, *args): 
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

class NotificationDaemon(object):
    """
    This class is a wrapper for notification-daemon program.
    """

    def __init__(self):
        bus = dbus.SessionBus()
        obj = bus.get_object('org.freedesktop.Notifications',
                '/org/freedesktop/Notifications')
        self.iface = dbus.Interface(obj, 'org.freedesktop.Notifications')


    # Main Message #######################################################

    def show(self, summary, message, icon, actions = {}): 
        if actions != {}:
            (notify_actions, action_handlers) = self.__process_actions(actions)

            def action_invoked(nid, action_id):
                if action_handlers.has_key(action_id) and res == nid:
                    #Execute the action handler
                    action_handlers[action_id]()

                self.iface.CloseNotification(dbus.UInt32(nid))

            self.iface.connect_to_signal("ActionInvoked", action_invoked)

        else:
            #Fixing no actions
            notify_actions = [(1, 2)]
            
        res = self.iface.Notify("Hermes", 
                [dbus.String(icon)],
                dbus.UInt32(0), 
                '', 
                1, 
                summary, 
                message, 
                [dbus.String(icon)],
                notify_actions,
                [(1,2)], 
                dbus.UInt32(10))
        return res


    # Specific messages #################################

    def show_info(self, summary, message, actions = {}):
        return self.show(summary, message, "gtk-dialog-info", actions)


    def show_warning(self, summary, message, actions = {}):
        return self.show(summary, message, "gtk-dialog-warning", actions)


    def show_error(self, summary, message):
        return self.show(summary, message, "gtk-dialog-error", actions)


    def close(self, nid):
        try:
            self.iface.CloseNotification(dbus.UInt32(nid))
        except:
            pass


    # Private methods ###################################
    def __process_actions(self, actions):
        """
        Devuelve una 2-tupla donde cada elemento es un diccionario.

        El primero contiene como claves los nombres de las acciones y como
        valores enteros los identificadores de la acción a tomar.

        El segundo contiene como claves los identificadores (enteros) de las
        acciones a tomar y como valores las funciones a ejecutar
        """
        if actions == {}:
            #FIXME
            return {}, {}

        for key in actions.keys():
            actions[" "] = None

        notify_actions = {}
        action_handlers = {}
        i = 1
        for key, value in actions.items():
            notify_actions[key] = i
            action_handlers[i] = value
            i += 1

        return notify_actions, action_handlers
        


def main():
    #Configure options
    parser = OptionParser(usage = 'usage: %prog [options]')
    parser.set_defaults(debug = False)

    parser.add_option('-d', '--debug', 
            action = 'store_true',
            dest = 'debug',
            help = 'start in debug mode')

    parser.add_option('-n', '--notification-daemon',
            action = 'store_true',
            dest = 'notification_daemon',
            help = 'Use notification-daemon as notification tool')

    (options, args) = parser.parse_args()
    del args
    
    if options.debug:
        level = logging.DEBUG
    else:
        level = logging.INFO

    logging.basicConfig(level = level,
            format='%(asctime)s %(levelname)s %(message)s',
                    filename='/var/tmp/hermes-hardware.log',
                    filemode='a')

    if options.notification_daemon:
        iface = NotificationDaemon()

    else:
        #Connect to dbus
        bus = dbus.SessionBus()
        object = bus.get_object("org.guadalinex.Hermes", "/org/guadalinex/HermesObject")
        iface = dbus.Interface(object, "org.guadalinex.IHermesNotifier")

    DeviceListener(iface)
    gtk.threads_init()
    gtk.main()


if __name__ == "__main__":
    main()


