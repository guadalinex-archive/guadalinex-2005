# -*- coding: utf8 -*-

ACTORSLIST = []
CATEGORIES = {}
BUSSES = {}

import os
import os.path
import logging
                
DIR=os.path.dirname(__file__) + os.sep 
file_list = [ele for ele in os.listdir(DIR) if os.path.isfile(DIR + os.sep + ele)]
logger = logging.getLogger()

for filename in file_list:

    if filename == '__init__.py' or filename.split('.')[-1] != 'py' or \
            filename  == 'deviceactor.py':
        continue
    
    module_name = filename.split('.')[0]
    try:
        actor_module = __import__(module_name, globals(), locals(),['*']) 
        ACTORSLIST.append(actor_module.Actor)

    except Exception, e:
        logger.warning("Actor %s not loading." % (module_name,))
