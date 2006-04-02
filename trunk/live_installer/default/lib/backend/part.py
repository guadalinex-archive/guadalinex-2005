# -*- coding: utf-8 -*-

# «part» - etapa de particionamiento
# 
# Copyright (C) 2005 Junta de Andalucía
# 
# Autor/es (Author/s):
# 
# - Juan Jesús Ojeda Croissier <juanje#interactors._coop>
# - Javier Carranza <javier.carranza#interactors._coop>
# - Antonio Olmo Titos <aolmo#emergya._info>
# 
# Este fichero es parte del instalador en directo de Guadalinex 2005.
# 
# El instalador en directo de Guadalinex 2005 es software libre. Puede
# redistribuirlo y/o modificarlo bajo los términos de la Licencia Pública
# General de GNU según es publicada por la Free Software Foundation, bien de la
# versión 2 de dicha Licencia o bien (según su elección) de cualquier versión
# posterior. 
# 
# El instalador en directo de Guadalinex 2005 se distribuye con la esperanza de
# que sea útil, pero SIN NINGUNA GARANTÍA, incluso sin la garantía MERCANTIL
# implícita o sin garantizar la CONVENIENCIA PARA UN PROPÓSITO PARTICULAR. Véase
# la Licencia Pública General de GNU para más detalles.
# 
# Debería haber recibido una copia de la Licencia Pública General junto con el
# instalador en directo de Guadalinex 2005. Si no ha sido así, escriba a la Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
# USA.
# 
# -------------------------------------------------------------------------
# 
# This file is part of Guadalinex 2005 live installer.
# 
# Guadalinex 2005 live installer is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the License, or
# at your option) any later version.
# 
# Guadalinex 2005 live installer is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with
# Guadalinex 2005 live installer; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

""" U{pylint<http://logilab.org/projects/pylint>} mark: -6.50!! (bad
    indentation) """

# Last modified by A. Olmo on 20 oct 2005.

from subprocess import *
from os import system

def call_autoparted (assistant, drive, progress = None):

  """ Perform automatic partition.
      @return: a dictionary containing a device for each mount point (i.e.
      C{{'/dev/hda5': '/', '/dev/hda7': '/home', '/dev/hda6': 'swap'}}). """

  return assistant.auto_partition (drive, steps = progress)

def percentage(per, num):
  re = (num * per)/100
  return re

def calc_sizes(tam):
  '''
     /      ->  2355 Mb > x < 20 Gb   -> 25 %
     /home  ->   512 Mb > x           ->  5 %
     swap   ->   205 Mb > x <  1 Gb   -> 70 %
  '''
  if tam < 3072:
    return None
    
  sizes = {}
  sizes['root'] = percentage(tam,25)
  sizes['swap'] = percentage(tam,5)
  #home = percentage(tam,70)

  if sizes['root'] < 2355:
    sizes['root'] = 2355
  elif sizes['root'] > 20480:
    sizes['root'] = 20480

  if sizes['swap'] < 205:
    sizes['swap'] = 205
  elif sizes['swap'] > 1024:
    sizes['swap'] = 1024

  sizes['home'] = tam - sizes['root'] - sizes['swap']
  if sizes['home'] < 512:
    return None

  return sizes
  
def call_all_disk(drive):
  # 1 - get the disk size
  out = Popen(['/sbin/sfdisk', '-s', drive], stdin=PIPE, stdout=PIPE,
              close_fds=True)
  tam = int(out.stdout.readline().strip())
  tam = tam/1024
  # 2 - call calc_sizes(tam)
  sizes = calc_sizes(tam)
  if not sizes:
    return False
  # 3 - to parte the disk using calcs
  cmd = "/sbin/sfdisk -uM %s << EOF\n,%d,L\n,%d,L\n,,S\nEOF" % (drive, sizes['root'], sizes['home'])
  try:
    ret = call(cmd, shell=True)
  except OSError, e:   
    print >>sys.stderr, "Execution failed:", e
    return False

  return True

def call_gparted(widget):

  '''call_autoparted() -> dict {'mount point' : 'dev'}
                       -> None
  '''
  import gtk
  import sys

  # plug/socket implementation (Gparted integration)
  socket = gtk.Socket()
  socket.show()
  widget.add(socket)
  Wid = str(socket.get_id())

  # TODO: rewrite next block.
  #mountpoints = None

  try:
    out = Popen(['/usr/bin/gparted', '-i', Wid], stdin=PIPE, stdout=PIPE,
                close_fds=True)
    # get the output last line 
    #line = out.readlines()[-1].strip()
  except:
    try:
      out = Popen(['/usr/local/bin/gparted', '-i', Wid], stdin=PIPE,
                  stdout=PIPE, close_fds=True)
      # get the output last line 
      line = out.readlines()[-1].strip()
    except:
      widget.destroy()
      return None

  #FIXME:We need to know how the mounpoints are showed up

  return out.pid

# vim:ai:et:sts=2:tw=80:sw=2:

