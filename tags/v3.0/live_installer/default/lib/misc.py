#!/usr/bin/python
# -*- coding: utf-8 -*-


def grub_dev(dev):
    """returns a device name in grub mode from a unix device name."""

    letter = {'a': '0', 'b': '1', 'c': '2', 'd': '3', 'e': '4',
          'f': '5', 'g': '6', 'h': '7', 'i': '8'}
    num   = {'1': '0', '2': '1', '3': '2', '4': '3', '5': '4',
          '6': '5', '7': '6', '8': '7', '9': '8'}

    ext = dev[7:]
    name = 'hd%s,%s' % (letter[ext[0]], num[ext[1:]])
    return name


def part_label(dev):
    """returns an user-friendly device name from an unix device name."""

    drive_type = {'hd': 'IDE/ATA', 'sd': 'USB/SCSI/SATA'}
    dev, ext = dev.lower(), dev[7:]
    try:
      if ( int(dev[8:]) > 4 ):
        partition_type = _('Logical')
      else:
        partition_type = _('Primary')
    except:
      partition_type = _('Unknown')
    try:
      name = _('Partition %s Disc %s %s (%s) [%s]') % (ext[1:], drive_type[dev[5:7]], ord(ext[0])-ord('a')+1, partition_type, dev[5:])
    except:
     """For empty strings, other disk types and disks without partitions, like md1"""
     name = '%s' % (dev[5:])
    return name


def make_yaboot_header(target, target_dev):
    """builds yaboot header config-file."""

    import os, re

    yaboot_conf = open(target + '/etc/yaboot.conf', 'w')
    timeout = 50

    partition_regex = re.compile(r'[0-9]+$')
    partition = partition.search(target_dev).group()

    device_regex = re.compile(r'/dev/[a-z]+')
    device = device_regex.search(target_dev).group()
    device_of = subprocess.Popen(['ofpath', device], stdout=subprocess.PIPE).communicate()[0]

    boot_pipe1 = subprocess.Popen(['/sbin/fdisk', '-l', device], stdout=subprocess.PIPE)
    boot_pipe2 = subprocess.Popen(['grep', 'Apple_Bootstrap'], stdin=boot_pipe1.stdout, stdout=subprocess.PIPE)
    boot_stdout = boot_pipe2.communicate()[0]
    boot_partition_regex = re.compile(r'/dev/[a-z]+[0-9]+')
    boot_partition = boot_partition_regex.search(boot_stdout).group()

    yaboot_conf.write(' \
    ## yaboot.conf generated by the Ubuntu installer \
    ## \
    ## run: "man yaboot.conf" for details. Do not make changes until you have!! \
    ## see also: /usr/share/doc/yaboot/examples for example configurations. \
    ## \
    ## For a dual-boot menu, add one or more of: \
    ## bsd=/dev/hdaX, macos=/dev/hdaY, macosx=/dev/hdaZ \
    \
    boot=%s \
    device=%s \
    partition=%s \
    root=%s \
    timeout=%s \
    install=/usr/lib/yaboot/yaboot \
    magicboot=/usr/lib/yaboot/ofboot \
    enablecdboot \
    ' % (boot_partition, device_of, partition, target_dev, timeout) )


def ex(*args):
    """runs args* in shell mode. Output status is taken."""

    import subprocess
    msg = ''
    for word in args:
      msg += str(word) + ' '
      
    try:
      status = subprocess.call(msg, shell=True)
    except IOError, e:
      pre_log('error', msg)
      pre_log('error', "OS error(%s): %s" % (e.errno, e.strerror))
      return False
    else:
      if status != 0:
        pre_log('error', msg)
        return False
      pre_log('info', msg)
      return True


def ret_ex(*args):
    import subprocess
    msg = ''
    for word in args:
      msg += str(word) + ' '
    try:
      proc = subprocess.Popen(args, stdout=subprocess.PIPE, close_fds=True)
    except IOError, e:
      pre_log('error', msg)
      pre_log('error', "I/O error(%s): %s" % (e.errno, e.strerror))
      return None
    else:
      pre_log('info', msg)
      return proc.stdout


def get_var():
  """gets install input data from vars file."""

  import cPickle
  file = open('/tmp/vars')
  var = cPickle.load(file)
  file.close()
  return var


def set_var(var):
  """sets install input data into a vars file."""

  import cPickle
  file = open('/tmp/vars', 'w')
  cPickle.dump(var, file, -1)
  file.close()


def pre_log(code, msg=''):
  """logs install messages into /var/log on live filesystem."""

  distro = open ('/etc/lsb-release').readline ().strip ().split ('=') [1].lower ()
  log_file = '/var/log/' + distro + '-express'

  import logging
  logging.basicConfig(level=logging.DEBUG,
                      format='%(asctime)s %(levelname)-8s %(message)s',
                      datefmt='%a, %d %b %Y %H:%M:%S',
                      filename = log_file,
                      filemode='a')
  eval('logging.%s(\'%s\')' % (code,msg))


def post_log(code, msg=''):
  """logs install messages into /var/log on installed filesystem."""

  distro = open ('/etc/lsb-release').readline ().strip ().split ('=') [1].lower ()
  log_file = '/target/var/log/' + distro + '-express'

  import logging
  logging.basicConfig(level=logging.DEBUG,
                      format='%(asctime)s %(levelname)-8s %(message)s',
                      datefmt='%a, %d %b %Y %H:%M:%S',
                      filename = log_file,
                      filemode='a')
  eval('logging.%s(\'%s\')' % (code,msg))


def get_progress(str):
  """gets progress percentaje of installing process from progress bar message."""

  num = int(str.split()[:1][0])
  text = ' '.join(str.split()[1:])
  return num, text


def get_partitions():
  """returns an array with fdisk output related to partition data."""

  import re

  # parsing partitions from the procfs
  # attetion with the output format. the partitions list is without '/dev/'
  partition_table = open('/proc/partitions').read()
  regex = re.compile('[sh]d[a-g][0-9]+')
  partition = regex.findall(partition_table)

  return partition


def get_filesystems():
  """returns a dictionary with a skeleton { device : filesystem }
  with data from local hard disks. Only swap and ext3 filesystems
  are available."""

  import re, subprocess
  device_list = {}

  # building device_list dicts from "file -s" output from get_partitions
  #   returned list (only devices formatted as ext3, fat, ntfs or swap are
  #   parsed).
  partition_list = get_partitions()
  for device in partition_list:
    device = '/dev/' + device
#    filesystem_pipe = subprocess.Popen(['file', '-s', device], stdout=subprocess.PIPE)
    filesystem_pipe = subprocess.Popen(['sfdisk', '-c', device[0:8], device[8:]], stdout=subprocess.PIPE)
    filesystem = filesystem_pipe.communicate()[0]
    #if re.match('.*((ext3)|(swap)|(extended)|(data)).*', filesystem, re.I):
    #  if 'ext3' in filesystem.split() or 'data' in filesystem.split() or 'extended' in filesystem.split():
    #    device_list[device] = 'ext3'
    #  elif 'swap' in filesystem.split():
    #    device_list[device] = 'swap'
    #  elif 'FAT' in filesystem.split():
    #    device_list[device] = 'vfat'
    #  elif 'NTFS' in filesystem.split():
    #    device_list[device] = 'ntfs'
    if '83' in filesystem:
      device_list[device] = 'ext3'
    elif '82' in filesystem:
      device_list[device] = 'swap'
    elif 'b' in filesystem or 'c' in filesystem:
      device_list[device] = 'vfat'
    elif '7' in filesystem:
      device_list[device] = 'ntfs'
  return device_list


# Bootloader stuff
def lilo_entries(file):
    global default
    lines = []
    for line in file:
        if line.startswith('default='):
            default = line.split('=')[1]
        elif line.startswith('image') or line.startswith('other'):
            yield lines
            lines = [line]
        elif not line.startswith('#'):
            lines.append(line)
    yield lines


def grub_entries(file):
    lines = []
    for line in file:
        if line.startswith('title'):
            yield lines
            lines = [line]
        elif not line.startswith('#'):
            lines.append(line)
    yield lines

# vim:ai:et:sts=2:tw=80:sw=2: