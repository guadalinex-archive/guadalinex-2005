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
import socket
from bbutils            import *
from xml.dom.ext.reader import PyExpat  #   python-xml
from xml.xpath          import Evaluate
from Ft.Xml             import MarkupWriter

PPPPEERCONF="/etc/ppp/peers/dsl-provider"

def bb_enable_iface_with_config(dev, bootproto, ip='', broadcast='', gateway='', netmask='', network=''):
    """
    * Example of static interface configuration via system-tools-backends

    Function call:
    (sysret, xmlcfg, xmlcfgout) = bb_enable_iface_with_config('eth1', 'none', '10.0.0.6',
                                                              '10.255.255.255', '10.0.0.1',
                                                              '255.255.255.0', '10.0.0.0')
    xml passed to system-tools-backends:
    
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
    
    * Example of dhcp interface configuration via system-tools-backend

    Function call:
    (sysret, xmlcfg, xmlcfgout) = bb_enable_iface_with_config('eth1', 'dhcp')

    xml passed to system-tools-backends:
    
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

def bb_update_resolvconf(dns1, dns2):
    """
    Update of dns configuration.
    system-tools-backend don't have an option to only update de resolv.conf and
    ubuntu I think is not using resolvconf package by default. Then I update
    directly /etc/resolv.conf
    """

    # Check first if dns1 and dns2 are IP address for security reasons
    # if not exception
    socket.inet_aton(dns1)
    socket.inet_aton(dns2)

    fileresolv = open("/etc/resolv.conf", "w")
    fileresolv.write("nameserver " + dns1 + "\n")
    fileresolv.write("nameserver " + dns2 + "\n")
    fileresolv.close()

def bb_papchap_conf(PPPuser, PPPpasswd, sys_output_file):
    """
    This configures PPPuser, PPPpasswd in /etc/ppp/{pap-secrets,chap-secrets}
    if the user/passwd exists already updates it with the new info
    """
    # Check the vars
    
    filechap = open('/etc/ppp/pap-secrets', 'a+')
    filepap  = open('/etc/ppp/chap-secrets', 'a+')

    # Erase PPPuser from {pap,chap}-secrets

    usertofind = "\"" + PPPuser + "\""
    
    chapnewtmp = tempfile.NamedTemporaryFile()
    papnewtmp  = tempfile.NamedTemporaryFile()

    for line in filechap:
        if (line.find(usertofind) == -1):
            chapnewtmp.write(line)
        
    for line in filepap:
        if (line.find(usertofind) == -1):
            papnewtmp.write(line)

    # Add PPPuser to {pap,chap}-secrets

    auth = "\"" + PPPuser + "\" * \"" + PPPpasswd + "\"\n"

    chapnewtmp.write(auth)
    papnewtmp.write(auth)
    chapnewtmp.flush()
    papnewtmp.flush()
        
    filechap = open("/etc/ppp/pap-secrets",  "w")
    filepap  = open("/etc/ppp/chap-secrets", "w")
    newchaps = open(chapnewtmp.name,  "r").read()
    newpaps  = open(papnewtmp.name, "r").read()

    for line in newchaps:
        filechap.write(line)
    for line in newpaps:
        filepap.write(line)

    filepap.close()
    filechap.close()
    
    os.chmod("/etc/ppp/pap-secrets", 0600)
    os.chmod("/etc/ppp/chap-secrets", 0600)
    os.system("chown root:root /etc/ppp/pap-secrets")
    os.system("chown root:root /etc/ppp/chap-secrets")
    sys_output_file.write("* /etc/ppp/*-secrets: " +
                          _("Configurados correctamente.") + "\n")

def bb_create_ppp_peer_conf(devcf, sys_output_file):
    """
    conf ppp peer pppoe/pppoa configuration
    """
    
    peerfile = open('/etc/ppp/peers/dsl-provider', 'w')
    
    if (devcf.param['ppp_noipdefault']):
        peerfile.write('noipdefault\n')
    if devcf.param['ppp_defaultroute']:
        peerfile.write('defaultroute\n')
    if devcf.param['ppp_replacedefaultroute']:
        peerfile.write('replacedefaultroute\n')
    if devcf.param['ppp_usepeerdns']:
        peerfile.write('usepeerdns\n')
    if devcf.param['ppp_noauth']:
        peerfile.write('noauth\n')
    if devcf.param['ppp_updetach']:
        peerfile.write('updetach\n')
    peerfile.write("user \'" + devcf.param['PPPuser'] + "'\n")
    if devcf.param['ppp_proto'] == 'PPPoE':
        peerfile.write('plugin rp-pppoe.so')
        if devcf.device_type.dt_id == '0001': # modems
            if (devcf.id == '0003') or (devcf.id == '0006'):
                # Alcatel Speedtouch and Vitelcom
                peerfile.write('\nnas0\n') # an extra newline
        if devcf.device_type.dt_id == '0002': # routers
            peerfile.write(" " + devcf.param['eth_to_use'] + '\n')
    if devcf.param['ppp_proto'] == 'PPPoA':
        peerfile.write('plugin pppoatm.so\n')
        peerfile.write(devcf.param['ppp_vpi'] + "." +
                       devcf.param['ppp_vci'] + '\n')
    if devcf.param['ppp_persist']:
        peerfile.write('persist\n')
    if devcf.param['ppp_novjccomp']:
        peerfile.write('novjccomp\n')
    if devcf.param['ppp_noaccomp']:
        peerfile.write('noaccomp\n')
    if devcf.param['ppp_nobsdcomp']:
        peerfile.write('nobsdcomp\n')
    if devcf.param['ppp_nodeflate']:
        peerfile.write('nodeflate\n')
    if devcf.param['ppp_nopcomp']:
        peerfile.write('nopcomp\n')
    if devcf.param['ppp_noccp']:
        peerfile.write('noccp\n')
    if devcf.param['ppp_novj']:
        peerfile.write('novj\n')
    peerfile.write('maxfail ' + devcf.param['ppp_maxfail'] + '\n')
    peerfile.write('mru ' + devcf.param['ppp_mru'] + '\n')
    peerfile.write('mtu ' + devcf.param['ppp_mtu'] + '\n')
    if (devcf.param['ppp_lcpechofailure'] != '0'):
        peerfile.write('lcp-echo-failure ' +
                       devcf.param['ppp_lcpechofailure'] + '\n')
    if (devcf.param['ppp_lcpechointerval'] != '0'):
        peerfile.write('lcp-echo-interval ' +
                       devcf.param['ppp_lcpechointerval'] + '\n')
    if devcf.param['ppp_debug']:
        peerfile.write('debug\n')
    peerfile.close()
    os.chmod(PPPPEERCONF, 0640)
    os.system("chown root:dip " + PPPPEERCONF)
    sys_output_file.write("* " + PPPPEERCONF + ": " +
                          _("Configurado correctamente.") + "\n")
    return BBNOERR

def bb_ppp_conf(devcf, sys_output_file):
    """
    Configures a pppoe/pppoa in a simple way depending also on the provider
    selected in the GUI
    Eagle modems use eagleconf.
    """
    
    if (devcf.id == '0002'):
        if (devcf.provider.prov_id == '0001'): # Telefonica de España
            # FIXME: Better to put this in a xml file
            if devcf.param['mod_conf'] == "monodinamic":
                isp = 'ES01'
                enc = 1
            else:
                isp = 'ES02'
                enc = 3
        pwd_enc = '1' # FIXME: test this
        # FIXME: vpi/vci of table vs general vpi/vci
        cmd = "/usr/sbin/eagleconfig \"--params=LINETYPE=00000001|VPI=%07d|VCI=%08d|ENC=%07d|ISP=%s|ISP_LOGIN=%s|ISP_PWD=%s|PWD_ENCRYPT=%s|STATIC_IP=none|UPDATE_DNS=%d|START_ON_BOOT=%d|USE_TESTCONNEC=0|EU_LANG=|FORCE_IF=auto|CMVEI=WO|CMVEP=WO\"" % (int(devcf.param['ppp_vpi']), int(devcf.param['ppp_vci']), enc, isp, devcf.param['PPPuser'], devcf.param['PPPpasswd'], pwd_enc, int(devcf.param['ppp_usepeerdns']), int(devcf.param['ppp_startonboot']))
        sys_output_file.write("Command:\n")
        sys_output_file.write(cmd)
        sys_output_file.write("\n")
        cmd += " >> " + sys_output_file.name + " 2>&1"
        os.system(cmd)
    else:
        bb_papchap_conf(devcf.param['PPPuser'], devcf.param['PPPpasswd'], sys_output_file)
        bb_create_ppp_peer_conf(devcf, sys_output_file)
        # 3.1) Load  modules (modprobe -q pppoe)
        # 4) Bring your connection up
        # 5) Configure staronboot
        # Return plog
    return BBNOERR
