import os

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

  partition_list = get_partitions()
  for device in partition_list:
    device = '/dev/' + device
    filesystem_pipe = subprocess.Popen(['sfdisk', '-c', device[0:8], device[8:]], stdout=subprocess.PIPE)

    filesystem = filesystem_pipe.communicate()[0]
    if '83' in filesystem:
      device_list[device] = 'ext3'
    elif '82' in filesystem:
      device_list[device] = 'swap'
    elif 'b' in filesystem or 'c' in filesystem:
      device_list[device] = 'vfat'
    elif '7' in filesystem:
      device_list[device] = 'ntfs'
  return device_list


fstab = open('/etc/fstab', 'r')
blacklist = []
import re
for line in fstab.readlines():
	list_columns = line.split()
	try:
		if list_columns[1] == '/' or list_columns[1] == '/home':
			blacklist.append(list_columns[0])
		if list_columns[1] == 'none' and list_columns[2] == 'swap':
			blacklist.append(list_columns[0])
                homing = re.compile('/media/*')
                is_true = homing.match(list_columns[1])
                if is_true:
                        blacklist.append(list_columns[0])
	except:
		pass
fstab.close()

fstab = open('/etc/fstab','a')

win_counter = 1

for new_device, fs in get_filesystems().items():
  if ( fs in ['vfat', 'ntfs'] ):
    passno = 2
    if fs == 'vfat' :
      options = 'rw,users,exec,sync,auto'
    else:
      options = 'utf8,users,exec,auto,umask=022,nls=utf-8'
    path = '/media/Windows%d' % win_counter
    if not os.path.exists(path):
      os.mkdir(path)
    win_counter += 1
    print >>fstab, '%s\t%s\t%s\t%s\t%d\t%d' % (new_device, path, fs, options, 0, passno)
  elif fs == 'ext3':
    passno = 2
    if (new_device in blacklist):
      pass
    else:
      options = 'defaults,users,exec,auto'
      path = '/media/%s%d' % (new_device[5:8],int(new_device[8:]))
      if not os.path.exists(path):
        os.mkdir(path)
      print >>fstab, '%s\t%s\t%s\t%s\t%d\t%d' % (new_device, path, fs, options, 0, passno)
  elif fs == 'swap':
    if (new_device in blacklist):
      pass
    else:
      options = 'sw'
      path = 'none'
      passno = 0
      print >>fstab, '%s\t%s\t%s\t%s\t%d\t%d' % (new_device, path, fs, options, 0, passno)



fstab.close()
