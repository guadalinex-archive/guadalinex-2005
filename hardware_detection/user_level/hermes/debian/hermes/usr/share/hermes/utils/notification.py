#!/usr/bin/python
# -*- coding: utf-8 -*-

import dbus
import thread

if getattr(dbus, "version", (0, 0, 0)) >= (0, 41, 0):
    import dbus.glib


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
                    thread.start_new_thread(action_handlers[action_id], ())

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


    def show_error(self, summary, message, actions = {}):
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