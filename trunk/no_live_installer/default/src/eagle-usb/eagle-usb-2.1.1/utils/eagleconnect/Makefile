# $Id: Makefile,v 1.8 2004/11/17 20:58:55 Tux Exp $
# Makefile for installing eagleconnect
# Author: Tux

-include ../../Makefile.common

all: userscripts

userscripts: clean
	if test ${USE_EAGLECONNECT} == 1 ; then \
		mkdir -p tmp ; \
		cat diagnostic.tcl | sed \
			-e "s|@SBIN_DIR@|$(SBINDIR)|g" \
			-e "s|@EU_EAGLECONNECT_DIR@|$(EU_EAGLECONNECT_DIR)|g" \
			> tmp/diagnostic.tcl ; \
		cat eagleconnect.tcl | sed \
			-e "s|@SBIN_DIR@|$(SBINDIR)|g" \
			-e "s|@EU_DIR@|$(EU_DIR)|g" \
			-e "s|@EU_EAGLECONNECT_DIR@|$(EU_EAGLECONNECT_DIR)|g" \
			> tmp/eagleconnect.tcl ; \
		cat reseau.tcl | sed \
			-e "s|@SBIN_DIR@|$(SBINDIR)|g" \
			-e "s|@EU_EAGLECONNECT_DIR@|$(EU_EAGLECONNECT_DIR)|g" \
			> tmp/reseau.tcl ; \
	fi

install:
	if test ${USE_EAGLECONNECT} == 1 ; then \
		$(INSTALL) -d $(SBINDIR) && \
		$(INSTALL) -d $(EU_EAGLECONNECT_DIR)/lang && \
		$(INSTALL) -d $(EU_EAGLECONNECT_DIR)/images && \
		$(INSTALL) -m 0755 parameagleconnect.tcl $(EU_EAGLECONNECT_DIR) && \
		$(INSTALL) -m 0755 tmp/diagnostic.tcl $(EU_EAGLECONNECT_DIR) && \
		$(INSTALL) -m 0755 tmp/eagleconnect.tcl $(SBINDIR) && \
		$(INSTALL) -m 0755 tmp/reseau.tcl $(EU_EAGLECONNECT_DIR) && \
		$(INSTALL) -m 0644 lang/fr.msg $(EU_EAGLECONNECT_DIR)/lang && \
		$(INSTALL) -m 0644 lang/doc_fr.txt $(EU_EAGLECONNECT_DIR)/lang && \
		$(INSTALL) -m 0644 images/fichierssysteme.ppm $(EU_EAGLECONNECT_DIR)/images && \
		$(INSTALL) -m 0644 images/fonts.ppm $(EU_EAGLECONNECT_DIR)/images && \
		if ! test -f $(EU_DIR)/eagleconnect.conf ; then \
			$(INSTALL) -m 0644 eagleconnect.conf $(EU_DIR)/eagleconnect.conf ; \
		fi ; \
	fi

uninstall:
	rm -f $(SBINDIR)/eagleconnect.tcl && \
	rm -Rf $(EU_EAGLECONNECT_DIR)

clean:
	rm -Rf tmp

distclean:

.PHONY: all userscripts install uninstall clean distclean

#***************************************************************************
# $Log: Makefile,v $
# Revision 1.8  2004/11/17 20:58:55  Tux
# - "make uninstall" should remove eagleconnect.tcl
#
# Revision 1.7  2004/09/28 20:26:57  emmanuel
# Images directory and pictures associated added
#
# Revision 1.6  2004/09/26 19:30:47  Tux
# - add $EU_DIR path
# - install eagleconnect documentation
#
# Revision 1.5  2004/08/25 21:10:37  Tux
# - changed eagleconnect.conf path
#
# Revision 1.4  2004/08/04 19:57:33  Tux
# - paths have been moved directly into .tcl scripts
# - (temporary?) removed expect script
#
# Revision 1.3  2004/08/03 17:33:57  Tux
# - now do something
#
# Revision 1.2  2004/08/01 20:35:18  Tux
# *** empty log message ***
#
# Revision 1.1  2004/08/01 20:31:53  Tux
# - Initial revision
#
#***************************************************************************/
