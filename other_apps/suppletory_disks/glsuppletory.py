#!/usr/bin/python
# -*- coding: utf-8 -*-
import logging
import os.path

from utils.synaptic import Synaptic
from volume import Actor


GLVALIDLABELS = [
    "Guadalinex.suppletory.disk",
    "Guadalinex.Suppletory.Disk",
    ]
#GAI (guadalinex-app-install) Packages

GAIPACKAGES = []
RELATIVEICONPATH = '.icon.png'



class GlSuppletory(object):
    """
    This class encapsule a volume.Actor hack which can detect Guadalinex
    Suppletory Disks.
    """

    def __init__(self):
        self.logger = logging.getLogger()

    def hack(self, volume_actor):
        """
        <volume_actor> must be a volume.Actor object
        """

        s = Synaptic()
        mountpoint = volume_actor.properties['volume.mount_point']

        def action_install_gai():
            s.install(GAIPACKAGES)
            self.show_supplement_info(volume_actor)

        def action_install_sup():
            os.system('guadalinex-suppletory-summoner ' + mountpoint)

        #Check for label and  README.diskdefines
        volumelabel = volume_actor.properties['volume.label']
        if self.__is_valid(volumelabel):
            s = Synaptic()
            actions = {}
            diskdefines = self.__get_diskdefines(volume_actor)
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
                    volume_actor.msg_render.show(summary, message, 
                            icon = iconpath, actions = actions)

                else:
                    volume_actor.msg_render.show_info(summary, message,
                            actions = actions)


                

    def show_supplement_info(self, volume_actor):
        ddpath = volume_actor.properties['volume.mount_point']
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


    def __get_diskdefines(self, volume_label):
        filepath = volume_label.properties['volume.mount_point'] + \
                '/README.diskdefines'

        try:
            fileobject = open(filepath)
        except Exception, e:
            self.logger.error(str(e))
            return {}

        result = {}
        for line in fileobject.readlines():
            items = line.split(None, 2)
            result[items[1]] = items[2]

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
