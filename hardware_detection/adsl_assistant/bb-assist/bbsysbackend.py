#!/usr/bin/env python
# -*- coding: UTF8 -*-

"""
bb-assist - A DSL Assistant Configurator

Copyright (C) 2005 Junta de Andalucía

Autor/es (Author/s):

- Vicente J. Ruiz Jurado <vjrj@tid.es>

Este fichero es parte de bb-assist.

bb-assist es software libre. Puede redistribuirlo y/o modificarlo
bajo los términos de la Licencia Pública General de GNU según es
publicada por la Free Software Foundation, bien de la versión 2 de dicha
Licencia o bien (según su elección) de cualquier versión posterior.
 
bb-assist se distribuye con la esperanza de que sea útil,
pero SIN NINGUNA GARANTÍA, incluso sin la garantía MERCANTIL
implícita o sin garantizar la CONVENIENCIA PARA UN PROPÓSITO
PARTICULAR. Véase la Licencia Pública General de GNU para más detalles.

Debería haber recibido una copia de la Licencia Pública General
junto con bb-assist. Si no ha sido así, escriba a la Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.

-------------------------------------------------------------------------

This file is part of bb-assist.

bb-assist is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
at your option) any later version.

bb-assist is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
"""

import os, sys, tempfile
from bbutils            import *
from xml.dom.ext.reader import PyExpat  #   python-xml
from xml.xpath          import Evaluate
from Ft.Xml             import MarkupWriter

def bb_enable_iface_with_config(dev, bootproto, ip='', broadcast='', gateway='', netmask='', network=''):
    """
    Example of static interface configuration via system-tools-backends
    
    <?xml version='1.0' encoding='UTF-8' standalone='yes'?>
    <interface type='ethernet'>
      <configuration>
        <address>10.0.0.8</address>
        <auto>1</auto>
        <bootproto>none</bootproto>
        <broadcast>10.0.0.255</broadcast>
        <file>eth1</file>
        <gateway>10.0.0.1</gateway>
        <netmask>255.255.255.0</netmask>
        <network>10.0.0.0</network>
      </configuration>
      <dev>eth1</dev>
      <enabled>1</enabled>
    </interface>
    <!-- GST: end of request -->
    
    *** Example of dhcp interface configuration via system-tools-backend

    <interface type='ethernet'>
       <configuration>
         <auto>1</auto>
         <bootproto>dhcp</bootproto>
         <file>eth0</file>
       </configuration>
       <dev>eth0</dev>
       <enabled>1</enabled>
    </interface>
    <!-- GST: end of request -->
    """
    
    netcfg_xmlfile = tempfile.NamedTemporaryFile()
    netcfgout_xmlfile = tempfile.NamedTemporaryFile()
    writer = MarkupWriter(netcfg_xmlfile, encoding='UTF-8', indent=u"yes", 
                          standalone="yes")
    writer.startDocument()
    #writer.startElement(u'!DOCTYPE network []')
    writer.startElement(u'interface')
    writer.attribute(u'type', u'ethernet')
    writer.startElement(u'configuration')
    if bootproto == 'dhcp':
        conf = { 'auto' : '1', 'bootproto' : bootproto, 'file': dev }
    else:
        conf = { 'address' : ip, 'auto' : '1', 'bootproto' :'none', 'broadcast' : broadcast,
                 'file': dev, 'gateway' : gateway, 'netmask' : netmask, 'network' : network }
    for confparam in conf.keys():
        writer.startElement(unicode(confparam))
        writer.text(unicode(conf[confparam]))
        writer.endElement(unicode(confparam))
    writer.endElement(u'configuration')
    writer.startElement(u'dev')
    writer.text(unicode(dev))
    writer.endElement(u'dev')
    writer.startElement(u'enabled')
    writer.text(u'1')
    writer.endElement(u'enable')
    writer.endElement(u'interface')
    writer.text(u'\n')
    writer.comment(u' GST: end of request ')
    writer.endDocument()
    netcfg_xmlfile.flush()

    xmlcfg = open(netcfg_xmlfile.name).read( )
    net_cfg_cmd = "cat " + netcfg_xmlfile.name + " | /usr/share/setup-tool-backends/scripts/network-conf -d enable_iface_with_config | grep -vE \"^$\" > " + netcfgout_xmlfile.name
    os.system(net_cfg_cmd)
    xmlcfgout = open(netcfgout_xmlfile.name).read( )
    reader = PyExpat.Reader( )
    dom = reader.fromStream(open(netcfgout_xmlfile.name, "r"))
    successCfg = Evaluate("success/text( )",
                         dom.documentElement)[0].nodeValue
    if successCfg == '0':
        sysret = bbutils.BBERRCFGDEV # Error configuration dev
    else:
        sysret = bbutils.BBNOERR # Ok
    
    return (sysret, xmlcfg, xmlcfgout)

if __name__ == "__main__":
    (sysret, xmlcfg, xmlcfgout) = bb_enable_iface_with_config('eth1', 'dhcp')
    print sysret
    print xmlcfgout
    print xmlcfg
    (sysret, xmlcfg, xmlcfgout) = bb_enable_iface_with_config('eth1', 'none', '10.0.0.6',
                                                              '10.255.255.255', '10.0.0.1',
                                                              '255.255.255.0', '10.0.0.0')
    print sysret
    print xmlcfgout
    print xmlcfg
