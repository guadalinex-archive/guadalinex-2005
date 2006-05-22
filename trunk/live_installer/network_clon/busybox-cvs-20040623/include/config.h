/*
 * Automatically generated header file: don't edit
 */

#define AUTOCONF_INCLUDED

/* Version Number */
#define BB_VER "1.00-pre10"
#define BB_BT "2006.04.12-10:18+0000"

#define HAVE_DOT_CONFIG 1

/*
 * General Configuration
 */
#undef CONFIG_FEATURE_BUFFERS_USE_MALLOC
#define CONFIG_FEATURE_BUFFERS_GO_ON_STACK 1
#undef CONFIG_FEATURE_BUFFERS_GO_IN_BSS
#define CONFIG_FEATURE_VERBOSE_USAGE 1
#undef CONFIG_FEATURE_INSTALLER
#undef CONFIG_LOCALE_SUPPORT
#undef CONFIG_FEATURE_DEVFS
#undef CONFIG_FEATURE_DEVPTS
#undef CONFIG_FEATURE_CLEAN_UP
#undef CONFIG_FEATURE_SUID
#undef CONFIG_SELINUX

/*
 * Build Options
 */
#define CONFIG_STATIC 1
#define CONFIG_LFS 1
#undef USING_CROSS_COMPILER
#define EXTRA_CFLAGS_OPTIONS ""

/*
 * Installation Options
 */
#define CONFIG_INSTALL_NO_USR 1
#define PREFIX "./_install"

/*
 * Archival Utilities
 */
#undef CONFIG_AR
#undef CONFIG_BUNZIP2
#undef CONFIG_CPIO
#undef CONFIG_DPKG
#undef CONFIG_DPKG_DEB
#undef CONFIG_GUNZIP
#undef CONFIG_GZIP
#undef CONFIG_RPM2CPIO
#undef CONFIG_RPM
#undef CONFIG_TAR
#undef CONFIG_UNCOMPRESS
#undef CONFIG_UNZIP

/*
 * Coreutils
 */
#define CONFIG_BASENAME 1
#undef CONFIG_CAL
#define CONFIG_CAT 1
#undef CONFIG_CHGRP
#define CONFIG_CHMOD 1
#undef CONFIG_CHOWN
#undef CONFIG_CHROOT
#define CONFIG_CMP 1
#define CONFIG_CP 1
#define CONFIG_CUT 1
#undef CONFIG_DATE
#undef CONFIG_DD
#undef CONFIG_DF
#undef CONFIG_DIRNAME
#undef CONFIG_DOS2UNIX
#undef CONFIG_DU
#define CONFIG_ECHO 1
#define CONFIG_FEATURE_FANCY_ECHO 1
#define CONFIG_ENV 1
#define CONFIG_EXPR 1
#define CONFIG_FALSE 1
#undef CONFIG_FOLD
#define CONFIG_HEAD 1
#define CONFIG_FEATURE_FANCY_HEAD 1
#undef CONFIG_HOSTID
#undef CONFIG_ID
#undef CONFIG_INSTALL
#undef CONFIG_LENGTH
#define CONFIG_LN 1
#undef CONFIG_LOGNAME
#define CONFIG_LS 1
#define CONFIG_FEATURE_LS_FILETYPES 1
#define CONFIG_FEATURE_LS_FOLLOWLINKS 1
#undef CONFIG_FEATURE_LS_RECURSIVE
#undef CONFIG_FEATURE_LS_SORTFILES
#undef CONFIG_FEATURE_LS_TIMESTAMPS
#undef CONFIG_FEATURE_LS_USERNAME
#undef CONFIG_FEATURE_LS_COLOR
#define CONFIG_MD5SUM 1
#define CONFIG_MKDIR 1
#define CONFIG_MKFIFO 1
#define CONFIG_MKNOD 1
#define CONFIG_MV 1
#undef CONFIG_OD
#define CONFIG_PRINTF 1
#define CONFIG_PWD 1
#undef CONFIG_REALPATH
#define CONFIG_RM 1
#define CONFIG_RMDIR 1
#undef CONFIG_SEQ
#undef CONFIG_SHA1SUM
#define CONFIG_SLEEP 1
#undef CONFIG_FEATURE_FANCY_SLEEP
#define CONFIG_SORT 1
#define CONFIG_STAT 1
#define CONFIG_FEATURE_STAT_FORMAT 1
#undef CONFIG_STTY
#define CONFIG_SYNC 1
#define CONFIG_TAIL 1
#undef CONFIG_FEATURE_FANCY_TAIL
#define CONFIG_TEE 1
#undef CONFIG_FEATURE_TEE_USE_BLOCK_IO
#define CONFIG_TEST 1

/*
 * test (forced enabled for use with shell)
 */
#define CONFIG_TOUCH 1
#define CONFIG_TR 1
#define CONFIG_TRUE 1
#define CONFIG_TTY 1
#define CONFIG_UNAME 1
#define CONFIG_UNIQ 1
#undef CONFIG_USLEEP
#undef CONFIG_UUDECODE
#undef CONFIG_UUENCODE
#undef CONFIG_WATCH
#undef CONFIG_WC
#undef CONFIG_WHO
#undef CONFIG_WHOAMI
#define CONFIG_YES 1

/*
 * Common options for cp and mv
 */
#undef CONFIG_FEATURE_PRESERVE_HARDLINKS

/*
 * Common options for ls and more
 */
#undef CONFIG_FEATURE_AUTOWIDTH

/*
 * Common options for df, du, ls
 */
#undef CONFIG_FEATURE_HUMAN_READABLE

/*
 * Common options for md5sum, sha1sum
 */
#define CONFIG_FEATURE_MD5_SHA1_SUM_CHECK 1

/*
 * Console Utilities
 */
#define CONFIG_CHVT 1
#define CONFIG_CLEAR 1
#define CONFIG_DEALLOCVT 1
#define CONFIG_DUMPKMAP 1
#define CONFIG_LOADFONT 1
#define CONFIG_LOADKMAP 1
#define CONFIG_OPENVT 1
#define CONFIG_RESET 1
#define CONFIG_SETKEYCODES 1

/*
 * Debian Utilities
 */
#define CONFIG_MKTEMP 1
#undef CONFIG_PIPE_PROGRESS
#define CONFIG_READLINK 1
#define CONFIG_FEATURE_READLINK_FOLLOW 1
#undef CONFIG_RUN_PARTS
#undef CONFIG_START_STOP_DAEMON
#undef CONFIG_WHICH

/*
 * Editors
 */
#undef CONFIG_AWK
#undef CONFIG_PATCH
#define CONFIG_SED 1
#define CONFIG_VI 1
#define CONFIG_FEATURE_VI_COLON 1
#define CONFIG_FEATURE_VI_YANKMARK 1
#define CONFIG_FEATURE_VI_SEARCH 1
#define CONFIG_FEATURE_VI_USE_SIGNALS 1
#define CONFIG_FEATURE_VI_DOT_CMD 1
#undef CONFIG_FEATURE_VI_READONLY
#define CONFIG_FEATURE_VI_SETOPTS 1
#define CONFIG_FEATURE_VI_SET 1
#define CONFIG_FEATURE_VI_WIN_RESIZE 1
#define CONFIG_FEATURE_VI_OPTIMIZE_CURSOR 1

/*
 * Finding Utilities
 */
#undef CONFIG_FIND
#define CONFIG_GREP 1
#define CONFIG_FEATURE_GREP_EGREP_ALIAS 1
#define CONFIG_FEATURE_GREP_FGREP_ALIAS 1
#define CONFIG_FEATURE_GREP_CONTEXT 1
#undef CONFIG_XARGS

/*
 * Init Utilities
 */
#undef CONFIG_INIT
#undef CONFIG_HALT
#undef CONFIG_POWEROFF
#define CONFIG_REBOOT 1
#undef CONFIG_MESG

/*
 * Login/Password Management Utilities
 */
#define CONFIG_USE_BB_PWD_GRP 1
#undef CONFIG_ADDGROUP
#undef CONFIG_DELGROUP
#undef CONFIG_ADDUSER
#undef CONFIG_DELUSER
#undef CONFIG_GETTY
#undef CONFIG_LOGIN
#undef CONFIG_PASSWD
#undef CONFIG_SU
#undef CONFIG_SULOGIN
#undef CONFIG_VLOCK

/*
 * Miscellaneous Utilities
 */
#undef CONFIG_ADJTIMEX
#undef CONFIG_CROND
#undef CONFIG_CRONTAB
#undef CONFIG_DC
#undef CONFIG_DEVFSD
#undef CONFIG_LAST
#undef CONFIG_HDPARM
#undef CONFIG_MAKEDEVS
#undef CONFIG_MT
#undef CONFIG_RX
#undef CONFIG_STRINGS
#undef CONFIG_TIME
#undef CONFIG_WATCHDOG

/*
 * Linux Module Utilities
 */
#define CONFIG_MODUTILS 1
#undef CONFIG_MODUTILS_OBJ
#define CONFIG_MODUTILS_OBJ_2_6 1
#undef CONFIG_DEPMOD
#define CONFIG_INSMOD 1
#undef CONFIG_FEATURE_2_4_MODULES
#define CONFIG_FEATURE_2_6_MODULES 1
#undef CONFIG_LSMOD
#undef CONFIG_MODPROBE
#undef CONFIG_RMMOD

/*
 * Networking Utilities
 */
#define CONFIG_FEATURE_IPV6 1
#undef CONFIG_ARPING
#define CONFIG_FTPGET 1
#undef CONFIG_FTPPUT
#undef CONFIG_HOSTNAME
#undef CONFIG_HTTPD
#define CONFIG_IFCONFIG 1
#define CONFIG_FEATURE_IFCONFIG_STATUS 1
#define CONFIG_FEATURE_IFCONFIG_SLIP 1
#define CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ 1
#define CONFIG_FEATURE_IFCONFIG_HW 1
#define CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS 1
#undef CONFIG_IFUPDOWN
#undef CONFIG_INETD
#define CONFIG_IP 1
#define CONFIG_FEATURE_IP_ADDRESS 1
#define CONFIG_FEATURE_IP_LINK 1
#define CONFIG_FEATURE_IP_ROUTE 1
#undef CONFIG_FEATURE_IP_TUNNEL
#undef CONFIG_IPCALC
#undef CONFIG_IPADDR
#undef CONFIG_IPLINK
#undef CONFIG_IPROUTE
#undef CONFIG_IPTUNNEL
#undef CONFIG_NAMEIF
#undef CONFIG_NC
#undef CONFIG_NETSTAT
#undef CONFIG_NSLOOKUP
#undef CONFIG_PING
#undef CONFIG_PING6
#define CONFIG_ROUTE 1
#undef CONFIG_TELNET
#undef CONFIG_TELNETD
#undef CONFIG_TFTP
#undef CONFIG_TRACEROUTE
#undef CONFIG_VCONFIG
#undef CONFIG_WGET

/*
 * udhcp Server/Client
 */
#undef CONFIG_UDHCPD
#define CONFIG_UDHCPC 1
#undef CONFIG_FEATURE_UDHCP_SYSLOG
#undef CONFIG_FEATURE_UDHCP_DEBUG

/*
 * Process Utilities
 */
#undef CONFIG_FREE
#define CONFIG_KILL 1
#undef CONFIG_KILLALL
#undef CONFIG_PIDOF
#define CONFIG_PS 1
#undef CONFIG_RENICE
#undef CONFIG_TOP
#undef CONFIG_UPTIME
#undef CONFIG_SYSCTL

/*
 * Another Bourne-like Shell
 */
#define CONFIG_FEATURE_SH_IS_ASH 1
#undef CONFIG_FEATURE_SH_IS_HUSH
#undef CONFIG_FEATURE_SH_IS_LASH
#undef CONFIG_FEATURE_SH_IS_MSH
#undef CONFIG_FEATURE_SH_IS_NONE
#define CONFIG_ASH 1

/*
 * Ash Shell Options
 */
#define CONFIG_ASH_JOB_CONTROL 1
#define CONFIG_ASH_ALIAS 1
#define CONFIG_ASH_MATH_SUPPORT 1
#define CONFIG_ASH_MATH_SUPPORT_64 1
#define CONFIG_ASH_GETOPTS 1
#define CONFIG_ASH_CMDCMD 1
#undef CONFIG_ASH_MAIL
#define CONFIG_ASH_OPTIMIZE_FOR_SIZE 1
#undef CONFIG_ASH_RANDOM_SUPPORT
#undef CONFIG_HUSH
#undef CONFIG_LASH
#undef CONFIG_MSH

/*
 * Bourne Shell Options
 */
#undef CONFIG_FEATURE_SH_EXTRA_QUIET
#define CONFIG_FEATURE_SH_STANDALONE_SHELL 1
#define CONFIG_FEATURE_COMMAND_EDITING 1
#define CONFIG_FEATURE_COMMAND_HISTORY 15
#define CONFIG_FEATURE_COMMAND_SAVEHISTORY 1
#define CONFIG_FEATURE_COMMAND_TAB_COMPLETION 1
#undef CONFIG_FEATURE_COMMAND_USERNAME_COMPLETION
#undef CONFIG_FEATURE_SH_FANCY_PROMPT

/*
 * System Logging Utilities
 */
#undef CONFIG_SYSLOGD
#undef CONFIG_LOGGER

/*
 * Linux System Utilities
 */
#undef CONFIG_DMESG
#define CONFIG_FBSET 1
#define CONFIG_FEATURE_FBSET_FANCY 1
#define CONFIG_FEATURE_FBSET_READMODE 1
#define CONFIG_FDFLUSH 1
#undef CONFIG_FDFORMAT
#undef CONFIG_FDISK
#define FDISK_SUPPORT_LARGE_DISKS 1
#undef CONFIG_FREERAMDISK
#undef CONFIG_FSCK_MINIX
#undef CONFIG_MKFS_MINIX
#undef CONFIG_GETOPT
#undef CONFIG_HEXDUMP
#undef CONFIG_HWCLOCK
#define CONFIG_LOSETUP 1
#define CONFIG_MKSWAP 1
#define CONFIG_MORE 1
#undef CONFIG_FEATURE_USE_TERMIOS
#undef CONFIG_PIVOT_ROOT
#undef CONFIG_RDATE
#define CONFIG_SWAPONOFF 1
#define CONFIG_MOUNT 1
#define CONFIG_NFSMOUNT 1
#define CONFIG_UMOUNT 1
#undef CONFIG_BLOCKDEV
#define CONFIG_FEATURE_MOUNT_FORCE 1

/*
 * Common options for mount/umount
 */
#define CONFIG_FEATURE_MOUNT_LOOP 1
#undef CONFIG_FEATURE_MTAB_SUPPORT

/*
 * Debugging Options
 */
#undef CONFIG_DEBUG
