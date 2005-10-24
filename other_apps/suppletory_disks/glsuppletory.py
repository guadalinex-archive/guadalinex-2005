#!/usr/bin/python
# -*- coding: utf-8 -*-
import logging
import os.path

from utils.synaptic import Synaptic
from volume import Actor


GLVALIDLABELS = [
    "Guadalinex.suppletory.disk",
    "Guadalinex.Suppletory.Disk",
    "GSD-"
    ]
#GAI (guadalinex-app-install) Packages

GAIPACKAGES = []
RELATIVEICONPATH = '.icon.png'
APTCONFPATH='/usr/share/gsd/apt.conf'
SOURCESFILE='/tmp/gsd/sources.list'
DISTRONAME='flamenco'


class GlSuppletory(object):
    """
    This class encapsule a volume.Actor hack which can detect Guadalinex
    Suppletory Disks.
    """

    def __init__(self):
        self.logger = logging.getLogger()
        #Current volume.Actor hacked object
        self.volume_actor = None

    def hack(self, volume_actor):
        """
        <volume_actor> must be a volume.Actor object
        """

        self.volume_actor = volume_actor
        s = Synaptic()
        mountpoint = self.volume_actor.properties['volume.mount_point']

        def action_install_gai():
            s.install(GAIPACKAGES)
            self.show_supplement_info()

        def action_install_sup():
            self.guadalinex_suppletory_summoner(mountpoint)

        #Check for label and  README.diskdefines
        volumelabel = self.volume_actor.properties['volume.label']
        if self.__is_valid(volumelabel):
            s = Synaptic()
            actions = {}
            diskdefines = self.__get_diskdefines()
            if diskdefines:
                #Check for required packages
                if s.check(GAIPACKAGES):
                    actions = {
                        "Instalar el suplemento": action_install_sup
                    }
                else:
                    actions = {
                        "Instalar herramientas para suplementos" : action_install_gai
                        }

                diskname = diskdefines['DISKNAME']
                message  = diskname
                summary = "Guadalinex Suplementos"
                iconpath = mountpoint + '/' + RELATIVEICONPATH
                if os.path.isfile(iconpath):
                    self.volume_actor.msg_render.show(summary, message, 
                            icon = iconpath, actions = actions)

                else:
                    self.volume_actor.msg_render.show_info(summary, message,
                            actions = actions)


    def guadalinex_suppletory_summoner(self, mountpoint):    
        """
        This method install suppletory.
        """

        os.system('gksudo -m "Introduzca contraseña" /bin/true')
        os.system('sudo cp -a /usr/share/gsd /tmp')

        #Generate sources.list
        self.__create_sources_list()

        #Exec app-install
        os.system('APT_CONFIG=%s sudo guadalinex-app-install %s' % \
                (APTCONFPATH, mountpoint ))

    def show_supplement_info(self):
        ddpath = self.volume_actor.properties['volume.mount_point']
        #parser = DiskDefinesParser()


    def hack_volume_actor(self):

        def new_on_modified(volume_actor, prop_name):
            Actor.old_on_modified(volume_actor, prop_name)
            if prop_name == 'volume.is_mounted' and \
                    volume_actor.properties['volume.is_mounted']:
                self.hack(volume_actor)
        
        #Hacking volume.Actor class
        Actor.old_on_modified = Actor.on_modified
        Actor.on_modified = new_on_modified


    def __create_sources_list(self):
        self.logger.debug('Creating sources.list')
        diskdefines = self.__get_diskdefines()
        fileobj = open(SOURCESFILE, 'w')
        for key in diskdefines.keys():
            if key.startswith('URI'):
                self.__process_uri(diskdefines[key], fileobj)
        fileobj.close()


    def __process_uri(self, value, fileobj):
        
        self.logger.debug('Processing uri: ' + value)
        if value.startswith('http://') or \
                value.startswith('fto://'):
            fileobj.write('deb ' + str(value) + '\n')

        else:
            mountpoint = self.volume_actor.properties['volume.mount_point']
            fileobj.write('deb file:' + mountpoint + value + \
                    ' '+ DISTRONAME +' main \n')


    def __get_diskdefines(self ):
        filepath = self.volume_actor.properties['volume.mount_point'] + \
                '/README.diskdefines'

        try:
            fileobject = open(filepath)
        except Exception, e:
            self.logger.error(str(e))
            return {}

        result = {}
        for line in fileobject.readlines():
            items = line.split(None, 2)
            try:
                result[items[1]] = items[2]
            except IndexError, e:
                result[items[1]] = ''


        return result

    
    def __is_valid(self, label):
        """
        Check if <labes> is a valid label for Guadalinex cd
        """

        for valid_label in GLVALIDLABELS:
            if label.startswith(valid_label):
                return True

        return False


    


gls = GlSuppletory()
gls.hack_volume_actor()
