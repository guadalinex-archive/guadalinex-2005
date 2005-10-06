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

import os, sys
import pexpect                          # Depends: python-pexpect
import serial                           #   python-serial
import re
from serial.serialutil import *
from xml.dom.ext.reader import PyExpat  #   python-xml
from xml.xpath          import Evaluate

CMDENCODING='iso-8859-1'

def func_parse(func):
    path_cmd = "cmd_func[@id='"+ func +"']/cmd"
    cmds = Evaluate(path_cmd, dom.documentElement)
    if (len(cmds) < 1):
        raise SyntaxError, _("en modulo de expect. Función call desconocida")
    return (cmd_parse(cmds))


def cmd_parse(cmds):
    for actcmd in cmds:
        send_cmd = actcmd.getAttribute('send')
        ret_cmd = actcmd.getAttribute('return')
        act_exp_ok = actcmd.getAttribute('exp_ok')
        call_cmd = actcmd.getAttribute('call')
        act_except = actcmd.getAttribute('on_excep')
        err = actcmd.getAttribute('err')
        
        if ret_cmd: type_cmd = 'return'
        elif call_cmd: type_cmd = 'call'
        elif (act_exp_ok): type_cmd = 'sendline'
        else: raise SyntaxError, _("en modulo de expect. Comando desconocido")

        if type_cmd == 'sendline':
            child.sendline(send_cmd)
            expect_list = []
            cmdid_list = []

            if err != '':
                expect_list += [re.compile(err.encode(CMDENCODING))]
                cmdid_list += ['__exit__']

            expect_list += [re.compile(act_exp_ok.encode(CMDENCODING))]
            cmdid_list += ['__plus1__']
            
            expects = Evaluate("expect_list/expect", actcmd)
            
            for expectact in expects:
                expect_list += [re.compile(expectact.getAttribute('out').encode(CMDENCODING))]
                inner_cmds = Evaluate('cmd', expectact)
                cmdid_list += [inner_cmds]

            cmd_timeout = actcmd.getAttribute('timeout')
            if cmd_timeout != '': act_timeout = int(cmd_timeout)
            else: 
                act_timeout = int(default_timeout)

            if act_except != '':
                expect_list += [pexpect.EOF, pexpect.TIMEOUT]
                cmdid_list += [act_except, act_except]
            
            expopt = child.expect_list(expect_list, timeout=act_timeout)

            if cmdid_list[expopt] == '__plus1__':
                pass
            elif cmdid_list[expopt] == act_except:
                sub_ret = func_parse(act_except)
                if sub_ret != 0:
                    # if return != 0 raise the error
                    return sub_ret
            elif cmdid_list[expopt] == '__exit__':
                sub_ret = func_parse('__exit__')
                if sub_ret != 0:
                    # if return != 0 raise the error
                    return sub_ret
            elif type(cmdid_list[expopt]) == list:
                sub_ret = cmd_parse(cmdid_list[expopt])
                if sub_ret != 0:
                    # if return != 0 raise the error
                    return sub_ret
        elif type_cmd == 'call':
            sub_ret = func_parse(call_cmd)
            if sub_ret != 0:
                # if return != 0 raise the error
                return sub_ret
        elif type_cmd == 'return':
            return(int(ret_cmd))
            
    # All commands ok, return 0
    return 0

reader = PyExpat.Reader( )
dom = reader.fromStream(sys.stdin)

cmd_ini         = Evaluate("initial_func/text( )",
                           dom.documentElement)[0].nodeValue
default_timeout = Evaluate("default_timeout/text( )",
                           dom.documentElement)[0].nodeValue

ethnode = Evaluate("eth_params", dom.documentElement)
if len(ethnode) == 1:
    by_serial = False
    eth_dev = ethnode[0].getAttribute('dev')
    ip = ethnode[0].getAttribute('ip')
    port = ethnode[0].getAttribute('port')
else:
    by_serial = True
    tty_read        = Evaluate("serial_params",
                               dom.documentElement)[0].getAttribute('tty')
    baudrate_read   = Evaluate("serial_params",
                               dom.documentElement)[0].getAttribute('baudrate')
    bits_read       = Evaluate("serial_params",
                               dom.documentElement)[0].getAttribute('bits')
    parity_read     = Evaluate("serial_params",
                               dom.documentElement)[0].getAttribute('parity')
    stopbits_read   = Evaluate("serial_params",
                               dom.documentElement)[0].getAttribute('stopbits')
    xonxoff_read    = Evaluate("serial_params",
                               dom.documentElement)[0].getAttribute('xonxoff')
    rtscts_read     = Evaluate("serial_params",
                               dom.documentElement)[0].getAttribute('rtscts')

if by_serial:
    if   (parity_read == "N") : parity_cte = PARITY_NONE
    elif (parity_read == "E") : parity_cte = PARITY_EVEN
    elif (parity_read == "O") : parity_cte = PARITY_ODD

    ser = serial.Serial(
        port     = int(tty_read),
        baudrate = int(baudrate_read),
        bytesize = int(bits_read),
        parity   = parity_cte,
        stopbits = int(stopbits_read),
        timeout  = None,               # set a timeout value, None to wait forever
        xonxoff  = int(xonxoff_read),  # enable software flow control
        rtscts   = int(rtscts_read),   # enable RTS/CTS flow control
        writeTimeout = None,          # set a timeout for writes
        )
    fd = ser.fd
    #    if os.path.exists("/var/lock/LCK..ttyS"+tty_read):
    #FIXME       raise Exception( _("Puerto serie bloqueado"))
    # FIXME: block tty, check if LCK process already exist
    child = pexpect.spawn(fd)
else:
    telnet_cmd = 'telnet ' + ip + " " + port
    child = pexpect.spawn(telnet_cmd)

fout = file('/tmp/bb-assist.log','a') # FIXME: clean
child.setlog (fout)

cmd_act = cmd_ini

ret = func_parse(cmd_act)
sys.exit(ret)
