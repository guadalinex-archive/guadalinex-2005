# $Id: Makefile,v 1.35 2005/02/13 23:32:58 baud123 Exp $

# Copyright (C) 2003      Jerome Marant
#				2003-2005 Olivier Borowski
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# Goal :
#	Makefile for installing user space scripts

-include ../../Makefile.common

all: userscripts

userscripts: clean
	mkdir -p tmp
	sed 	-e "s|@SBIN_DIR@|$(SBINDIR)|g" \
		-e "s|@EU_DIR@|$(EU_DIR)|g" \
		-e "s|@EU_LANG_DIR@|$(EU_LANG_DIR)|g" \
		-e "s|@USE_HOTPLUG@|$(USE_HOTPLUG)|g" \
		-e "s|@USE_IFUPDOWN@|$(USE_IFUPDOWN)|g" \
		-e "s|@BOOT_METHOD@|$(BOOT_METHOD)|g" \
		-e "s|@DISTRIB@|$(DISTRIB)|g" \
		-e "s|@DISTVER@|$(DISTVER)|g" \
		-e "s|@SIMPLE@|$(SIMPLE)|g" \
		-e "s|@INIT_DIR@|$(INIT_DIR)|g" \
		-e "s|@PPP_DIR@|$(PPP_DIR)|g" \
		-e "s|@HOTPLUG_SCRIPT_DIR@|$(HOTPLUG_SCRIPT_DIR)|g" \
		-e "s|@NET_SCRIPT_DIR@|$(NET_SCRIPT_DIR)|g" \
		-e "s|@EU_LANG@|$(EU_LANG)|g" \
		-e "s|@PPP_OPTIONS_DIR@|$(PPP_OPTIONS_DIR)|g" \
		-e "s|@PPP_OPTIONS_ADSL@|$(PPP_OPTIONS_ADSL)|g" \
		-e "s|@PPP_OPTIONS_MIRE@|$(PPP_OPTIONS_MIRE)|g" \
		-e "s|@SYSCONF_FILE@|$(SYSCONF_FILE)|g" \
		<setvars >tmp/setvars && \
	sed -e "s|exit 123|. $(EU_DIR)/setvars|" -e "s| 1 -eq 1 | 0 -eq 1 |" fctStopAdsl > tmp/fctStopAdsl && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" fctStartAdsl > tmp/fctStartAdsl && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" startadsl > tmp/startadsl && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" stopadsl > tmp/stopadsl && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" eagleconfig > tmp/eagleconfig && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" eaglediag > tmp/eaglediag && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" eu_config_bash > tmp/eu_config_bash && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" eu_dsp > tmp/eu_dsp && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" eu_init > tmp/eu_init && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" net_cnx_up > tmp/net_cnx_up && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" net_cnx_down > tmp/net_cnx_down && \
	sed "s|exit 123|. $(EU_DIR)/setvars|g" testconnec > tmp/testconnec

install:
	if test $(USE_IFUPDOWN) -eq 1 ; then \
		$(INSTALL) -d $(NET_SCRIPT_DIR) && \
		$(INSTALL) -m 0755 tmp/net_cnx_up $(NET_SCRIPT_DIR) && \
		$(INSTALL) -m 0755 tmp/net_cnx_down $(NET_SCRIPT_DIR) && \
		$(INSTALL) -m 0755 net_cnx_pg $(NET_SCRIPT_DIR) ; \
	fi
	$(INSTALL) -d $(EU_DIR) && \
	$(INSTALL) -d $(SBINDIR) && \
	$(INSTALL) -d $(EU_LANG_DIR) && \
	$(INSTALL) -m 0755 tmp/eaglediag $(SBINDIR) && \
	$(INSTALL) -m 0755 tmp/eagleconfig $(SBINDIR) && \
	$(INSTALL) -m 0755 tmp/eu_config_bash $(SBINDIR)/eagleconfig_front.bash && \
	$(INSTALL) -m 0755 tmp/fctStartAdsl $(SBINDIR) && \
	$(INSTALL) -m 0755 tmp/fctStopAdsl $(SBINDIR) && \
	$(INSTALL) -m 0755 tmp/startadsl $(SBINDIR) && \
	$(INSTALL) -m 0755 tmp/stopadsl $(SBINDIR) && \
	$(INSTALL) -m 0755 tmp/testconnec $(SBINDIR)/eagletestconnec && \
	$(INSTALL) -m 0644 tmp/setvars $(EU_DIR) && \
	$(INSTALL) -m 0644 options $(EU_DIR)/ppp_options.template && \
	$(INSTALL) -m 0644 lang/de $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/en $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/es $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/fr $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/f2 $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/it $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/mk $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/pl $(EU_LANG_DIR) && \
	$(INSTALL) -m 0644 lang/p2 $(EU_LANG_DIR) && \
	if test $(USE_HOTPLUG) -eq 1 ; then \
		$(INSTALL) -d $(HOTPLUG_SCRIPT_DIR) && \
		$(INSTALL) -m 0755 tmp/eu_dsp $(HOTPLUG_SCRIPT_DIR)/eagle-usb ; \
	fi
	# use eagle-usb service on Fedora, Redhat, Suse, Debian, Mandrake>=10.1
	if [ "$(BOOT_METHOD)" = "chkconfig" ] || [ "$(BOOT_METHOD)" = "update-rc.d" ] ; then \
		$(INSTALL) -d $(INIT_DIR) && \
		$(INSTALL) -m 0755 tmp/eu_init $(INIT_DIR)/eagle-usb ; \
	elif [ "$(BOOT_METHOD)" = "internet_service" ] ; then \
		$(INSTALL) -d $(INIT_DIR) && \
		$(INSTALL) -m 0755 tmp/eu_init $(INIT_DIR)/eagle-usb ; \
	elif [ "$(BOOT_METHOD)" = "slackware" ] ; then \
		$(INSTALL) -m 0755 tmp/eu_init $(EU_DIR)/rc.eagle-usb ; \
	fi
	touch $(EU_DIR)/eagle-usb_must_be_configured && \
	chmod 644 $(EU_DIR)/eagle-usb_must_be_configured

uninstall:
	./fctStopAdsl -sf
	# disable [and remove] autostart service
	if test -e $(INIT_DIR)/eagle-usb ; then \
		chkconfig --del eagle-usb &>/dev/null ; \
		update-rc.d -f eagle-usb remove &>/dev/null ; \
		rm -f $(INIT_DIR)/eagle-usb ; \
	fi
	if test -e $(INIT_DIR)/internet ; then \
		chkconfig --del internet &>/dev/null ; \
	fi
	# remove old eagle-usb files (previous versions)
	# avoid having multiple versions of the driver (for Mdk for example)
	#./uninstOld $(MODULESDIR) $(KERNELSRC) $(DISTRIB) $(DISTVER) $(BUILD_MODULE)
	./uninstOld
	rm -f $(SBINDIR)/eagleconfig && \
	rm -f $(SBINDIR)/eagleconfig_front.bash && \
	rm -f $(SBINDIR)/eaglediag && \
	rm -f $(SBINDIR)/eagletestconnec && \
	rm -f $(SBINDIR)/fctStartAdsl && \
	rm -f $(SBINDIR)/fctStopAdsl && \
	rm -f $(SBINDIR)/startadsl && \
	rm -f $(SBINDIR)/stopadsl && \
	rm -f $(EU_DIR)/eagle-usb_must_be_configured && \
	rm -f $(EU_DIR)/ppp_options.template && \
	rm -f $(EU_DIR)/setvars && \
	rm -f $(HOTPLUG_SCRIPT_DIR)/eagle-usb && \
	rm -f /etc/cron.d/eagle-usb && \
	rm -f /etc/rc.d/rc.eagle-usb && \
	rm -Rf $(EU_LANG_DIR)

clean:
	rm -Rf tmp

distclean:

.PHONY: all userscripts install uninstall clean distclean

#***************************************************************************
# $Log: Makefile,v $
# Revision 1.35  2005/02/13 23:32:58  baud123
# s/0ld/Old/ to avoid w4r10rd words
#
# Revision 1.34  2005/02/13 23:06:09  baud123
# put back uninst0ld to avoid existing driver
#
# Revision 1.33  2005/01/16 22:04:34  Tux
# - add license header
#
# Revision 1.32  2004/11/21 15:32:43  Tux
# - replaced == with -eq
#
# Revision 1.31  2004/11/17 20:58:14  Tux
# - change paths
#
# Revision 1.30  2004/11/17 16:41:31  mcoolive
# - (FHS compliance) move setvars in /etc/eagle-usb and change its permissions
#
# Revision 1.29  2004/11/11 16:21:26  mcoolive
# - renaming $EU_SCRIPT_DIR/lock in $EU_DIR/eagle-usb_must_be_configured
#
# Revision 1.28  2004/11/07 20:03:29  baud123
# add install for mk language
#
# Revision 1.27  2004/10/10 14:39:24  Tux
# *** empty log message ***
#
# Revision 1.26  2004/10/10 14:01:44  Tux
# - removed unused usb.usermap
#
# Revision 1.25  2004/10/10 13:53:28  Tux
# - removed startmire
# - eu_config_bash => eagleconfig_bash
#
# Revision 1.24  2004/09/28 19:45:20  Tux
# - add $BOOT_METHOD
#
# Revision 1.23  2004/09/23 20:39:49  Tux
# - use generic boot method detection
# - remove slackware specific init script (use generic one)
#
# Revision 1.22  2004/09/16 20:48:24  Tux
# - removed useless $SYSCONF_DIR variable
#
# Revision 1.21  2004/09/15 19:25:03  Tux
# - make sure that parent folders of the lock file exist
#
# Revision 1.20  2004/09/14 20:44:56  Tux
# - Mandrakelinux >=10.1 now uses eagle-usb service too
#
# Revision 1.19  2004/08/29 23:54:59  Tux
# - create directory /etc/init.d
#
# Revision 1.18  2004/08/29 21:18:36  Tux
# - install eu_init itself (instead of eagleconfig)
#
# Revision 1.17  2004/08/27 09:16:22  mcoolive
# - create hotplug/usb directory if it is needed
#
# Revision 1.16  2004/08/26 21:42:59  Tux
# - install eu_dsp script (previously eagleconfig did that)
#
# Revision 1.15  2004/08/14 23:41:22  mcoolive
# - to ensure of the access permissions of lock file
#
# Revision 1.14  2004/08/14 23:22:07  Tux
# - removed unuseful&dangerous umask
#
# Revision 1.13  2004/08/01 20:13:20  Tux
# - removed some unused hotplug related vars
#
# Revision 1.12  2004/07/16 21:14:54  Tux
# - added $PPP_OPTIONS_ADSL, $PPP_OPTIONS_MIRE, $SYSCONF_FILE
#
# Revision 1.11  2004/07/15 20:25:33  Tux
# - uninstall did't remove /etc/cron.d/eagle-usb
#
# Revision 1.10  2004/05/16 19:27:39  Tux
# - language files permissions: 0755 => 0644
#
# Revision 1.9  2004/04/21 20:07:52  Tux
# *** empty log message ***
#
# Revision 1.8  2004/04/21 18:19:21  Tux
# - some changes with ">/dev/null"
#
# Revision 1.7  2004/04/03 16:13:51  Tux
# - added eu_config_bash
#
# Revision 1.6  2004/03/22 21:18:29  Tux
# do not ask anymore the user to unplug the modem unless it is necessary
#
#***************************************************************************/
