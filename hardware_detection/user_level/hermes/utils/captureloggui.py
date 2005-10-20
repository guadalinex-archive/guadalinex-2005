#!/usr/bin/python
# -*- coding: utf-8 -*-

import gtk
import gtk.glade
import sys
import tempfile
import logging
import os.path
import webbrowser

class CaptureLogGui(object):

    def __init__(self):
        self.logger = logging.getLogger()

        #Set logfile
        self.__set_logfile()

        #Set interface
        xml = gtk.glade.XML('utils/captureloggui.glade', root = 'mainwindow')
        xml.signal_autoconnect(self)

        self.mainwindow = xml.get_widget('mainwindow')
        self.mainwindow.show_all()



    # SIGNAL HANDLERS ##################
    def on_capturelog(self, *args):
        #Close file an restore stdout
        self.logfile.close()

        content = open(self.filepath).read()

        newfile = open(os.path.expanduser('~') + \
                '/Desktop/nuevodispositivo.log', 
                'w')

        newfile.write(content)
        newfile.close()

        self.__set_logfile()
        

    def on_cancel(self, *args):
        gtk.main_quit()


    def on_finish(self, *args):
        gtk.main_quit()


    def on_help_clicked(self, *args):
        webbrowser.open("http://www.juntadeandalucia.es/repositorio/webs/guadalinex2005/doku.php?id=recogida_de_informacion_para_hermes")


    # Private methods ####################
    def __set_logfile(self):
        self.filepath = tempfile.mktemp()

        self.logfile = open(self.filepath, 'w')
        sys.stdout = self.logfile





