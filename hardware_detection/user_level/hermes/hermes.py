#!/usr/bin/python
# -*- coding: utf8 -*- 

import gtk
import dbus
import egg.trayicon
import thread
import gobject
import os
import logging

from optparse import OptionParser

class DefaultMessageRender:

    def __init__(self):
        pass


    def show_info(self, message):
        print "Info: ", message

        
    def show_warning(self, message):
        print "Warning: ", message


    def show_error(self, message):
        print "Error: ", message


    def show_question(self, message, default = 1):
        print "Question: ", message
        return default


    def show_entry(self, message):
        print "Entry: ", message
        return "Texto de la entrada"



class HermesTray (egg.trayicon.TrayIcon):

    def __init__(self):
        egg.trayicon.TrayIcon.__init__(self, "VT")
        #self.add(gtk.Label("VT"))
        img = gtk.Image()
        #img.set_from_stock(gtk.STOCK_DIALOG_INFO, gtk.ICON_SIZE_MENU)
        img.set_from_file('img' + os.sep + 'trayicon.png')

        self.add(img)

        self.dlg = None


    def show_message(self, message, msg_type):
        thread.start_new_thread(self.__show_message, (message, msg_type))


    def __show_message (self, message, msg_type):
        self.abort()
        #Mostramos el trayicon
        gtk.gdk.threads_enter()
        self.show_all()
        gtk.gdk.threads_leave()

        gtk.gdk.threads_enter()
        dlg = gtk.MessageDialog(parent = self,
                                type = msg_type,
                                flags = gtk.DIALOG_MODAL,
                                message_format = message,
                                buttons = gtk.BUTTONS_NONE)

        self.__setup_dialog(dlg)
        gtk.gdk.threads_leave()


        def timeout():
            """
            Timeout handler than hide messagedialog
            """
            dlg.response(0)
            return False #Para que sólo se ejecute una vez


        def timeout_2():
            """
            Timeout handler than hide trayicon
            """
            self.hide_all()
            return False
        
        gtk.gdk.threads_enter()
        gobject.timeout_add(3000, timeout)
        gobject.timeout_add(5000, timeout_2)

        dlg.run()
        dlg.destroy()
        while gtk.events_pending():
            gtk.main_iteration()
        gtk.gdk.threads_leave()


    def show_question(self, question, default = 1):
        #Mostramos el trayicon
        gtk.gdk.threads_enter()
        self.show_all()

        dlg = gtk.MessageDialog(parent = self,
                                type = gtk.MESSAGE_QUESTION,
                                flags = gtk.DIALOG_MODAL,
                                message_format = question,
                                buttons = gtk.BUTTONS_YES_NO)

        self.__setup_dialog(dlg)

        def timeout():
            """
            Timeout handler than hide messagedialog
            """
            if default == 0:
                dlg.response(gtk.RESPONSE_NO)
            else:
                dlg.response(gtk.RESPONSE_YES)
            return False #Para que sólo se ejecute una vez

        def timeout_2():
            """
            Timeout handler that hide trayicon
            """
            self.hide_all()
            return False
        
        gobject.timeout_add(7000, timeout)
        gobject.timeout_add(9000, timeout_2)

        if dlg.run() == gtk.RESPONSE_YES:
            res = 1 
        else:
            res = 0
        
        dlg.destroy()
        self.hide_all()

        while gtk.events_pending():
            gtk.main_iteration()

        gtk.gdk.threads_leave()
        return res


    def show_entry(self, message):
        """
        Muestra un diálogo con una entrada de texto para que el usuario
        introduzca algún dato

        Devuelve el texto introducido por el usuario
        """
        gtk.gdk.threads_enter()
        self.show_all()

        dlg = gtk.Dialog( "Entrada",
                parent = self,
                flags = gtk.DIALOG_MODAL,
                buttons = (gtk.STOCK_APPLY, gtk.RESPONSE_OK))

        entry = gtk.Entry()

        dlg.vbox.pack_start(gtk.Label(message))
        dlg.vbox.pack_start(entry)
        dlg.vbox.show_all()

        self.__setup_dialog(dlg)

        def timeout():
            """
            Timeout handler than hide messagedialog
            """
            dlg.response(gtk.RESPONSE_CANCEL)
            return False #Para que sólo se ejecute una vez

        def timeout_2():
            """
            Timeout handler than hide trayicon
            """
            self.hide_all()
            return False
        
        gobject.timeout_add(10000, timeout)
        gobject.timeout_add(11000, timeout_2)

        dlg.run() 
        res =  entry.get_text()
        
        dlg.destroy()
        self.hide_all()

        while gtk.events_pending():
            gtk.main_iteration()

        gtk.gdk.threads_leave()
        return res


    def abort(self):
        gtk.gdk.threads_enter()
        try:
            self.dlg.destroy()
        except Exception:
            pass
        gtk.gdk.threads_leave()


    def __setup_dialog(self, dialog):
        dialog.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_decorated(False) #Sin borde
        dialog.stick() #The message dialog shows in all workspaces
        dialog.set_keep_above(True) #Se mantiene en primer plano
        dialog.set_focus_on_map(False)
        dialog.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(65500, 65535, 48573))

        self.dlg = dialog


class HermesTray2:

    def __init__(self):
        self.trayicon = egg.trayicon.TrayIcon("VT")
        img = gtk.Image()
        img.set_from_file('img' + os.sep + 'trayicon.png')

        self.trayicon.add(img)
        
        self.main_window = gtk.Dialog(parent = self.trayicon, flags = gtk.DIALOG_MODAL)
        self.__setup_dialog(self.main_window)

        #Delete all children
        for child in self.main_window.vbox.get_children():
            self.main_window.vbox.remove(child)

        self.box = self.main_window.vbox


        self.box.set_resize_mode(gtk.RESIZE_IMMEDIATE)


    def show_message(self, message, msg_type):
        thread.start_new_thread(self.__show_message, (message, msg_type))


    def __show_message (self, message, msg_type):
        gtk.threads_enter()
        dlg = gtk.MessageDialog(parent = self.main_window,
                                type = msg_type,
                                flags = gtk.DIALOG_MODAL,
                                message_format = message,
                                buttons = gtk.BUTTONS_NONE)

        self.__setup_dialog(dlg)
        vbox = dlg.vbox
        dlg.remove(vbox)
        self.box.pack_end(vbox)
        vbox.show_all()
        gtk.threads_leave()

        def timeout():
            """
            Timeout handler than hide messagedialog
            """
            self.__remove_message(vbox)
            return False #Para que sólo se ejecute una vez


        def timeout_2():
            """
            Timeout handler than hide trayicon
            """
            self.trayicon.hide_all()
            return False
        
        gtk.gdk.threads_enter()
        gobject.timeout_add(3000, timeout)
        gobject.timeout_add(5000, timeout_2)

        self.trayicon.show_all()

        self.__setup_dialog(self.main_window)
        self.main_window.show_all()

        gtk.gdk.threads_leave()


    def __remove_message(self, vbox):
        self.box.remove(vbox)
        vbox.destroy()
        if len(self.box.get_children()) == 0:
            self.main_window.hide_all()

    def show_question(self, question, default = 1):
        #Mostramos el trayicon
        gtk.gdk.threads_enter()
        self.trayicon.show_all()

        dlg = gtk.MessageDialog(parent = self.trayicon,
                                type = gtk.MESSAGE_QUESTION,
                                flags = gtk.DIALOG_MODAL,
                                message_format = question,
                                buttons = gtk.BUTTONS_YES_NO)

        self.__setup_dialog(dlg)

        def timeout():
            """
            Timeout handler than hide messagedialog
            """
            if default == 0:
                dlg.response(gtk.RESPONSE_NO)
            else:
                dlg.response(gtk.RESPONSE_YES)
            return False #Para que sólo se ejecute una vez

        def timeout_2():
            """
            Timeout handler that hide trayicon
            """
            self.trayicon.hide_all()
            return False
        
        gobject.timeout_add(7000, timeout)
        gobject.timeout_add(9000, timeout_2)

        if dlg.run() == gtk.RESPONSE_YES:
            res = 1 
        else:
            res = 0
        
        dlg.destroy()

        while gtk.events_pending():
            gtk.main_iteration()

        gtk.gdk.threads_leave()
        return res


    def show_entry(self, message):
        """
        Muestra un diálogo con una entrada de texto para que el usuario
        introduzca algún dato

        Devuelve el texto introducido por el usuario
        """
        gtk.gdk.threads_enter()
        self.show_all()

        dlg = gtk.Dialog( "Entrada",
                parent = self,
                flags = gtk.DIALOG_MODAL,
                buttons = (gtk.STOCK_APPLY, gtk.RESPONSE_OK))

        entry = gtk.Entry()

        dlg.vbox.pack_start(gtk.Label(message))
        dlg.vbox.pack_start(entry)
        dlg.vbox.show_all()

        self.__setup_dialog(dlg)

        def timeout():
            """
            Timeout handler than hide messagedialog
            """
            dlg.response(gtk.RESPONSE_CANCEL)
            return False #Para que sólo se ejecute una vez

        def timeout_2():
            """
            Timeout handler than hide trayicon
            """
            self.hide_all()
            return False
        
        gobject.timeout_add(10000, timeout)
        gobject.timeout_add(11000, timeout_2)

        dlg.run() 
        res =  entry.get_text()
        
        dlg.destroy()
        self.hide_all()

        while gtk.events_pending():
            gtk.main_iteration()

        gtk.gdk.threads_leave()
        return res


    def abort(self):
        gtk.gdk.threads_enter()
        try:
            self.dlg.destroy()
        except Exception:
            pass
        gtk.gdk.threads_leave()


    def __setup_dialog(self, dialog):
        dialog.set_gravity(gtk.gdk.GRAVITY_SOUTH_EAST) 
        dialog.set_resizable(False)

        dialog.set_focus_on_map(False)
        dialog.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
        dialog.set_decorated(False) #Sin borde
        dialog.stick() #Message dialog shows in all workspaces
        dialog.set_keep_above(True) #Se mantiene en primer plano
        dialog.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(65500, 65535, 48573))

        self.dlg = dialog

class TrayObject(dbus.Object):

    def __init__(self, service, message_render = DefaultMessageRender()):
        dbus.Object.__init__(self, "/org/guadalinex/TrayObject", service)
        self.message_render = message_render
        self.logger = logging.getLogger()
        self.logger.debug("TrayObject iniciado")


    @dbus.method("org.guadalinex.TrayInterface")
    def show_info(self, message):
        """
        This method shows a info message
        """
        self.logger.info("show_info: " + message)
        return self.message_render.show_message(message, gtk.MESSAGE_INFO)


    @dbus.method("org.guadalinex.TrayInterface")
    def show_warning(self, message):
        """
        This method shows a info message
        """
        self.logger.info("show_warning: " + message)
        return self.message_render.show_message(message, gtk.MESSAGE_WARNING)


    @dbus.method("org.guadalinex.TrayInterface")
    def show_error(self, message):
        """
        This method shows a info message
        """
        self.logger.info("show_error: " + message)
        return self.message_render.show_message(message, gtk.MESSAGE_ERROR)


    @dbus.method("org.guadalinex.TrayInterface")
    def show_question(self, question, default = 1):
        """
        This method show a yes/no question. If default == 1, the default 
        answer is Yes and if default == 0 the default answer is No
        """
        self.logger.info("show_question: " + message)
        return self.message_render.show_question(question, default)


    @dbus.method("org.guadalinex.TrayInterface")
    def show_entry(self, message):
        return self.message_render.show_entry(message)


    @dbus.method("org.guadalinex.TrayInterface")
    def abort(self):
        """
        This method hide the message window.
        """
        return self.message_render.abort()


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
                    filename='/var/tmp/hermes-notifier.log',
                    filemode='a')

    logger = logging.getLogger()

    tray = HermesTray2()
    session_bus = dbus.SessionBus()
    service = dbus.Service("org.guadalinex.TrayService", bus = session_bus)
    TrayObject(service, tray)

    gtk.gdk.threads_init()
    logger.info("Started")
    gtk.main()


if __name__ == "__main__":
    main()
