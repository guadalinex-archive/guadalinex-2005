# The code here should not use debconf. This allows the generalised guts of
# operations such as OS probing to be used by non-debconf-ish programs on
# the installed system; plus, separating guts from user interface makes the
# postinst easier to read.

newline="
"

log=/var/log/messages

log() {
	logger -t grub-installer "$@"
}

error() {
	log "error: $@"
}

info() {
	log "info: $@"
}

# This is copied from update-grub. We've requested that it be moved
# to a utility or shell library.
device_map=/target/boot/grub/device.map
# Usage: convert os_device
# Convert an OS device to the corresponding GRUB drive.
# This part is OS-specific.
convert () {
#    # First, check if the device file exists.
#    if test -e "$1"; then
#		:
#    else
#		echo "$1: Not found or not a block device." 1>&2
#		exit 1
#    fi

	host_os=`uname -s | tr '[A-Z]' '[a-z]'`

    # Break the device name into the disk part and the partition part.
    case "$host_os" in
    linux*)
		tmp_disk=`echo "$1" | sed -e 's%\([sh]d[a-z]\)[0-9]*$%\1%' \
				  -e 's%\(fd[0-9]*\)$%\1%' \
				  -e 's%/part[0-9]*$%/disc%' \
				  -e 's%\(c[0-7]d[0-9]*\).*$%\1%'`
		tmp_part=`echo "$1" | sed -e 's%.*/[sh]d[a-z]\([0-9]*\)$%\1%' \
				  -e 's%.*/fd[0-9]*$%%' \
				  -e 's%.*/floppy/[0-9]*$%%' \
				  -e 's%.*/\(disc\|part\([0-9]*\)\)$%\2%' \
				  -e 's%.*c[0-7]d[0-9]*p*%%'`
	;;
    gnu*)
		tmp_disk=`echo "$1" | sed 's%\([sh]d[0-9]*\).*%\1%'`
		tmp_part=`echo "$1" | sed "s%$tmp_disk%%"` ;;
    freebsd*)
		tmp_disk=`echo "$1" | sed 's%r\{0,1\}\([saw]d[0-9]*\).*$%r\1%' \
			    | sed 's%r\{0,1\}\(da[0-9]*\).*$%r\1%'`
		tmp_part=`echo "$1" \
	    		| sed "s%.*/r\{0,1\}[saw]d[0-9]\(s[0-9]*[a-h]\)%\1%" \
       	    	| sed "s%.*/r\{0,1\}da[0-9]\(s[0-9]*[a-h]\)%\1%"`
	;;
    netbsd*)
		tmp_disk=`echo "$1" | sed 's%r\{0,1\}\([sw]d[0-9]*\).*$%r\1d%' \
	    		| sed 's%r\{0,1\}\(fd[0-9]*\).*$%r\1a%'`
		tmp_part=`echo "$1" \
	    		| sed "s%.*/r\{0,1\}[sw]d[0-9]\([abe-p]\)%\1%"`
	;;
    *)
		echo "update-grub does not support your OS yet." 1>&2
		exit 1 ;;
    esac

    # Get the drive name.
    tmp_drive=`grep -v '^#' $device_map | grep "$tmp_disk *$" \
			| sed 's%.*\(([hf]d[0-9][a-g0-9,]*)\).*%\1%'`

    # If not found, print an error message and exit.
    if test "x$tmp_drive" = x; then
		echo "$1 does not have any corresponding BIOS drive." 1>&2
		exit 1
    fi

    if test "x$tmp_part" != x; then
		# If a partition is specified, we need to translate it into the
		# GRUB's syntax.
		case "$host_os" in
		linux*)
	    	echo "$tmp_drive" | sed "s%)$%,`expr $tmp_part - 1`)%" ;;
		gnu*)
	    	if echo $tmp_part | grep "^s" >/dev/null; then
				tmp_pc_slice=`echo $tmp_part \
		    		| sed "s%s\([0-9]*\)[a-g]*$%\1%"`
				tmp_drive=`echo "$tmp_drive" \
		    		| sed "s%)%,\`expr "$tmp_pc_slice" - 1\`)%"`
	    	fi
	    	if echo $tmp_part | grep "[a-g]$" >/dev/null; then
				tmp_bsd_partition=`echo "$tmp_part" \
		    		| sed "s%[^a-g]*\([a-g]\)$%\1%"`
				tmp_drive=`echo "$tmp_drive" \
		    		| sed "s%)%,$tmp_bsd_partition)%"`
	    	fi
	    	echo "$tmp_drive" ;;
		freebsd*)
	    	if echo $tmp_part | grep "^s" >/dev/null; then
				tmp_pc_slice=`echo $tmp_part \
		    		| sed "s%s\([0-9]*\)[a-h]*$%\1%"`
				tmp_drive=`echo "$tmp_drive" \
		    		| sed "s%)%,\`expr "$tmp_pc_slice" - 1\`)%"`
	    	fi
	    	if echo $tmp_part | grep "[a-h]$" >/dev/null; then
				tmp_bsd_partition=`echo "$tmp_part" \
		    		| sed "s%s\{0,1\}[0-9]*\([a-h]\)$%\1%"`
				tmp_drive=`echo "$tmp_drive" \
		    		| sed "s%)%,$tmp_bsd_partition)%"`
	    	fi
	    	echo "$tmp_drive" ;;
		netbsd*)
	    	if echo $tmp_part | grep "^[abe-p]$" >/dev/null; then
				tmp_bsd_partition=`echo "$tmp_part" \
		    		| sed "s%\([a-p]\)$%\1%"`
				tmp_drive=`echo "$tmp_drive" \
		    		| sed "s%)%,$tmp_bsd_partition)%"`
	    	fi
	    	echo "$tmp_drive" ;;
		esac
    else
		# If no partition is specified, just print the drive name.
		echo "$tmp_drive"
    fi
}

# Convert a linux non-devfs disk device name into the hurd's syntax.
hurd_convert () {
	dr_type=$(expr "$1" : '.*\([hs]d\)[a-h][0-9]*')
	dr_letter=$(expr "$1" : '.*d\([a-h]\)[0-9]*')
	dr_part=$(expr "$1" : '.*d[a-h]\([0-9]*\)')
	case "$dr_letter" in
	a) dr_num=0 ;;
	b) dr_num=1 ;;
	c) dr_num=2 ;;
	d) dr_num=3 ;;
	e) dr_num=4 ;;
	f) dr_num=5 ;;
	g) dr_num=6 ;;
	h) dr_num=7 ;;
	esac
	echo "$dr_type${dr_num}s$dr_part"
}

get_serial_console() {
	local defconsole="$(sed -e 's/.*console=/console=/' /proc/cmdline)"
	if echo "${defconsole}" | grep -q console=ttyS; then
		local PORT="$(echo "${defconsole}" | sed -e 's%^console=ttyS%%' -e 's%,.*%%')"
		local SPEED="$(echo "${defconsole}" | sed -e 's%^console=ttyS[0-9]\+,%%' -e 's% .*%%')"
		local SERIAL="${PORT},${SPEED}"
		echo "console=$SERIAL"
	fi
}

# Make sure mtab in the chroot reflects the currently mounted partitions.
update_mtab() {
	mtab=/target/etc/mtab
	grep /target /proc/mounts | (
	while read devpath mountpoint fstype options n1 n2 ; do
		devpath=`mapdevfs $devpath || echo $devpath`
		mountpoint=`echo $mountpoint | sed s%^/target%%`
		# The sed line removes the mount point for root.
		if [ -z "$mountpoint" ] ; then
			mountpoint="/"
		fi
		echo $devpath $mountpoint $fstype $options $n1 $n2
	done ) > $mtab
}

# Run update-grub in /target. The caller should handle errors.
update_grub () {
	# Pipe from 'yes' to tell grub to create menu.lst when it is missing. A
	# better correct fix is to get update-grub to be able to run in
	# noninteractive mode.  Newer versions of the grub package support
	# '-y' for noninteractive installs.  But we'll keep it as-is for
	# Skolelinux.
	chroot /target /usr/bin/yes | chroot /target /sbin/update-grub >> $log 2>&1
}

is_floppy () {
	echo "$1" | grep -q '(fd' || echo "$1" | grep -q "/dev/fd" || echo "$1" | grep -q floppy
}

findfstype () {
	mount | grep "on /target${1%/} " | cut -d' ' -f5
}

# Make sure os-prober has been run. Called by other probe_* functions as
# necessary.
probe_init () {
	if [ ! -e /tmp/os-probed ]; then
		os-prober > /tmp/os-probed || true
		trap 'rm -f /tmp/os-probed' EXIT HUP INT QUIT TERM
	fi
}

# Work out what probed OSes can be booted from grub.
# Return codes:
#   0 - only the system we've just installed is present
#   1 - other systems are present, but are all supported
#   2 - other systems are present, and are not all supported
probe_can_boot () {
	probe_init

	# Work out what probed OSes can be booted from grub.
	tmpfile=/tmp/menu.lst.extras
	if [ ! -s /tmp/os-probed ]; then
		return 0
	fi

	supported_os_list=""
	unsupported_os_list=""

	OLDIFS="$IFS"
	IFS="$newline"
	for os in $(cat /tmp/os-probed); do
		IFS="$OLDIFS"
		title=$(echo "$os" | cut -d: -f2)
		type=$(echo "$os" | cut -d: -f4)
		case "$type" in
			chain)
				:
			;;
			linux)
				# Check for linux systems that we don't
				# know how to boot.
				partition=$(echo "$os" | cut -d: -f1)
				if [ -z "$(linux-boot-prober $partition)" ]; then
					if [ -n "$unsupported_os_list" ]; then
						unsupported_os_list="$unsupported_os_list, $title"
					else
						unsupported_os_list="$title"
					fi
					continue
				fi
			;;
			hurd)
				:
			;;
			*)
				if [ -n "$unsupported_os_list" ]; then
					unsupported_os_list="$unsupported_os_list, $title"
				else
					unsupported_os_list="$title"
				fi
				continue
			;;
		esac

		if [ -n "$supported_os_list" ]; then
			supported_os_list="$supported_os_list, $title"
		else
			supported_os_list="$title"
		fi
		
		IFS="$newline"
	done
	IFS="$OLDIFS"
	
	if [ -n "$unsupported_os_list" ]; then
		return 2
	else
		return 1
	fi
}

probe_menu_list () {
	probe_init

	# Generate menu.lst additions for other OSes.
	serial="$(get_serial_console)"
	tmpfile=/tmp/menu.lst.extras
	OLDIFS="$IFS"
	IFS="$newline"
	for os in $(cat /tmp/os-probed); do
		IFS="$OLDIFS"
		title=$(echo "$os" | cut -d: -f2)
		shortname=$(echo "$os" | cut -d: -f3)
		type=$(echo "$os" | cut -d: -f4)
		case "$type" in
			chain)
				partition=$(mapdevfs $(echo "$os" | cut -d: -f1))
				grubdrive=$(convert "$partition") || true
				if [ -n "$grubdrive" ]; then
					cat >> $tmpfile <<EOF

# This entry automatically added by the Debian installer for a non-linux OS
# on $partition
title		$title
root		$grubdrive
savedefault
EOF
					# Only set makeactive if grub is installed
					# in the mbr.
					if [ "$bootdev" = "(hd0)" ]; then
						cat >> $tmpfile <<EOF
makeactive
EOF
					fi
					# DOS/Windows can't deal with booting from a
					# non-first hard drive.
					case $shortname in
						MS*|Win*)
							grubdisk="$(echo "$grubdrive" | sed 's/^(//; s/)$//; s/,.*//')"
							case $grubdisk in
								hd0)	;;
								hd*)
									cat >> $tmpfile <<EOF
map		(hd0) ($grubdisk)
map		($grubdisk) (hd0)
EOF
									;;
							esac
							;;
					esac
					cat >> $tmpfile <<EOF
chainloader	+1

EOF
				fi
			;;
			linux)
				partition=$(echo "$os" | cut -d: -f1)
				mappedpartition=$(mapdevfs "$partition")
				IFS="$newline"
				for entry in $(linux-boot-prober "$partition"); do
					IFS="$OLDIFS"
					bootpart=$(echo "$entry" | cut -d: -f2)
					mappedbootpart=$(mapdevfs "$bootpart") || true
					if [ -z "$mappedbootpart" ]; then
						mappedbootpart="$bootpart"
					fi
					label=$(echo "$entry" | cut -d : -f3)
					if [ -z "$label" ]; then
						label="$title"
					fi
					kernel=$(echo "$entry" | cut -d : -f4)
					initrd=$(echo "$entry" | cut -d : -f5)
					if echo "$kernel" | grep -q '^/boot/' && \
					   [ "$mappedbootpart" != "$mappedpartition" ]; then
						# separate /boot partition
						kernel=$(echo "$kernel" | sed 's!^/boot!!')
						initrd=$(echo "$initrd" | sed 's!^/boot!!')
						grubdrive=$(convert "$mappedbootpart") || true
					else
						grubdrive=$(convert "$mappedpartition") || true
					fi
					params="$(echo "$entry" | cut -d : -f6-) $serial"
					cat >> $tmpfile <<EOF

# This entry automatically added by the Debian installer for an existing
# linux installation on $mappedpartition.
title		$label (on $mappedpartition)
root		$grubdrive
kernel		$kernel $params
EOF
					if [ -n "$initrd" ]; then
						cat >> $tmpfile <<EOF
initrd		$initrd
EOF
					fi
					cat >> $tmpfile <<EOF
savedefault
boot

EOF
					IFS="$newline"
				done
				IFS="$OLDIFS"
			;;
			hurd)
				partition=$(mapdevfs $(echo "$os" | cut -d: -f1))
				grubdrive=$(convert "$partition") || true
				hurddrive=$(hurd_convert "$partition") || true
				# Use the standard hurd boilerplate to boot it.
				cat >> $tmpfile <<EOF

# This entry automatically added by the Debian installer for an existing
# hurd installation on $partition.
title		$title (on $partition)
root		$grubdrive
kernel		/boot/gnumach.gz root=device:$hurddrive
module		/hurd/ext2fs.static --readonly \\
			--multiboot-command-line=\${kernel-command-line} \\
			--host-priv-port=\${host-port} \\
			--device-master-port=\${device-port} \\
			--exec-server-task=\${exec-task} -T typed \${root} \\
			\$(task-create) \$(task-resume)
module		/lib/ld.so.1 /hurd/exec \$(exec-task=task-create)
savedefault
boot

EOF
			;;
			*)
				info "unhandled: $os"
			;;
		esac
		IFS="$newline"
	done
	IFS="$OLDIFS"

	if [ -s $tmpfile ]; then
		cat >> /target/boot/grub/menu.lst << EOF

# This is a divider, added to separate the menu items below from the Debian
# ones.
title		Other operating systems:
root

EOF
		cat $tmpfile >> /target/boot/grub/menu.lst
		rm -f $tmpfile
	fi
}

# Install GRUB into $1. The caller should handle errors.
grub_install () {
	if ! is_floppy "$1"; then
		if chroot /target /sbin/grub-install -h 2>&1 | grep -q no-floppy; then
			info "grub-install supports --no-floppy"
			floppyparam="--no-floppy"
		else
			info "grub-install does not support --no-floppy"
		fi
	fi

	info "Running chroot /target /sbin/grub-install --recheck $floppyparam \"$1\""
	chroot /target /sbin/grub-install --recheck $floppyparam "$1" >> $log 2>&1
}

fix_kernel_package () {
	sed -e 's/do_bootloader = yes/do_bootloader = no/' < /target/etc/kernel-img.conf > /target/etc/kernel-img.conf.$$
	if [ -z "`grep update-grub /target/etc/kernel-img.conf.$$`" ]; then
		(
			echo "postinst_hook = /sbin/update-grub"
			echo "postrm_hook   = /sbin/update-grub"
		) >> /target/etc/kernel-img.conf.$$
	fi
	mv /target/etc/kernel-img.conf.$$ /target/etc/kernel-img.conf
}
