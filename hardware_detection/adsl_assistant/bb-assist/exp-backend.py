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
import pexpect
from xml.dom.ext.reader import PyExpat
from xml.xpath          import Evaluate

CMDENCODING='iso-8859-1'

reader = PyExpat.Reader( )
dom = reader.fromStream(sys.stdin)

tty             = Evaluate("serial_params",
                           dom.documentElement)[0].getAttribute('tty')
baudrate        = Evaluate("serial_params",
                           dom.documentElement)[0].getAttribute('baudrate')
bits            = Evaluate("serial_params",
                           dom.documentElement)[0].getAttribute('bits')
parity          = Evaluate("serial_params",
                           dom.documentElement)[0].getAttribute('parity')
stopbits        = Evaluate("serial_params",
                           dom.documentElement)[0].getAttribute('stopbits')

cmd_ini         = Evaluate("initial_cmd/text( )",
                           dom.documentElement)[0].nodeValue
default_timeout = Evaluate("default_timeout/text( )",
                           dom.documentElement)[0].nodeValue

fd = os.open('/dev/ttyS'+tty, os.O_RDWR|os.O_NOCTTY)
if os.path.exists("/var/lock/LCK..ttyS"+tty):
  raise Exception( _("Puerto serie bloqueado"))
#FIXME: block tty, check if LCK process already exist

#FIXME: open with correct tty params

child = pexpect.spawn(fd)
fout = file('/tmp/bb-assist.log','a') # FIXME: clean
child.setlog (fout)

cmd_act = cmd_ini
fin_cmds = False
while not fin_cmds:
  path_cmd = "cmd[@id="+ cmd_act +"]"
  print cmd_act
  cmds = Evaluate(path_cmd, dom.documentElement)
  if (len(cmds) <> 1):
    raise SyntaxError, _("en entrada al modulo de expect")
  actcmd = cmds[0]
  type_cmd = actcmd.getAttribute('type')
  if type_cmd == 'sendline':
    sendcmd = actcmd.getAttribute('cmd')
    child.sendline(sendcmd)
    expects = Evaluate("expect_list/expect", actcmd)
    expect_list = [pexpect.EOF, pexpect.TIMEOUT]
    cmdid_list = ['5000', '5001']
    for expectact in expects:
      expect_list += [expectact.getAttribute('out').encode(CMDENCODING)]
      cmdid_list += [expectact.getAttribute('nextcmdid').encode(CMDENCODING)]
    cmd_timeout = actcmd.getAttribute('timeout')
    if cmd_timeout != '':
      act_timeout = int(cmd_timeout)
    else:
      act_timeout = int(default_timeout)
    expopt = child.expect(expect_list, timeout=act_timeout)
    cmd_act=cmdid_list[expopt]
  elif type_cmd == 'exit':
    fin_cmds= True
    print actcmd.getAttribute('return')
