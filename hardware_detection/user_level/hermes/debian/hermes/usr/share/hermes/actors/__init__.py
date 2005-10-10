# -*- coding: utf8 -*-

ACTORSLIST = []
CATEGORIES = {}
BUSSES = {}


import os
import os.path

                
DIR=os.path.dirname(__file__) + os.sep 
file_list = [ele for ele in os.listdir(DIR) if os.path.isfile(DIR + os.sep + ele)]

for filename in file_list:
    #Si el nombre del fichero es __init__.py pasamos de
    if filename == '__init__.py' or filename.split('.')[-1] != 'py' or \
            filename  == 'deviceactor.py':
        continue
    
    module_name = filename.split('.')[0]
    try:
        actor_module = __import__(module_name, globals(), locals(),['*']) 

        ACTORSLIST.append(actor_module.Actor)

        #if actor_module.Actor.bus:
        #    BUSSES[actor_module.Actor.bus] = actor_module.Actor
        #if actor_module.Actor.category:
        #    CATEGORIES[actor_module.Actor.category] = actor_module.Actor
    except Exception, e:
        print "Error: ", e




    
