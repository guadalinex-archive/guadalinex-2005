# $Id: Makefile,v 1.38 2005/01/30 00:58:56 Tux Exp $

# Copyright (C) 2003      Jerome Marant
#               2003-2005 Olivier Borowski
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
#	Main makefile for the Linux driver for Eagle-based USB ADSL Modems

-include Makefile.common

# Messages:
SEP_MSG="==============================================================================="
ROOT_MSG="\n\nYou have to be root to do that!\n"
AUTOGEN_MSG="\n\nYou have to run ./autogen.sh first!\n"
CONFIGURE_MSG="\n\nYou have to run ./configure first!\n"
INSTALL_OK_MSG="\n\nInstallation has finished!\nYou should now run eagleconfig to setup your connexion.\n\n"
UNINSTALL_OK_MSG="\n\nUninstall has finished."
UNINSTALL_OK2_MSG="\n\n********************************************************************\n"\
"* You should now unplug and replug your modem to clear its memory. *\n"\
"********************************************************************\n\n"
MISS_PATH_MSG="warning: your PATH does not contain ${SBINDIR}. You won't be able to launch scripts.\n"\
"\t If you are using bash, you should update /root/.bashrc\n"


VERSION := $(shell cat VERSION)
ARCHNAME:= eagle-usb-${VERSION}

all: build

build:
	@if [ ! -e configure ] ; then \
		echo -e $(AUTOGEN_MSG) ; \
		exit 1 ; \
	fi
	@if [ ! -e Makefile.common ] ; then \
		echo -e $(CONFIGURE_MSG) ; \
		exit 1 ; \
	fi
	$(MAKE) -C driver
	$(MAKE) -C pppoa
	$(MAKE) -C utils/scripts
	$(MAKE) -C utils/eagleconnect
	@if [ "x${GEN_DOC}" = "xyes" ] ; then \
		$(MAKE) -C doc ; \
	fi

install:
	@if [ ! -e configure ] ; then \
		echo -e $(AUTOGEN_MSG) ; \
		exit 1 ; \
	fi
	@if [ ! -e Makefile.common ] ; then \
		echo -e $(CONFIGURE_MSG) ; \
		exit 1 ; \
	fi
	$(MAKE) -C driver install
	$(MAKE) -C pppoa install
	$(MAKE) -C utils/scripts install
	$(MAKE) -C utils/eagleconnect install
	# TODO : are manpages sometimes gzipped?? => update this test
	@if [ -t doc/man/eagleconfig.8 ] ; then \
		$(MAKE) -C doc install ; \
	fi
	#hash -r
	# check whether PATH contains {prefix}/sbin
	@test `id -ur` -ne 0 || echo ${PATH} | grep -q ${SBINDIR} || echo -e ${MISS_PATH_MSG}
	@echo -e $(SEP_MSG) && echo -e $(INSTALL_OK_MSG)

uninstall:
	@if [ `id -ur` -ne 0 ] ; then \
		echo -e $(ROOT_MSG) ; \
		exit 1 ; \
	fi
	@if [ ! -e configure ] ; then \
		echo -e $(AUTOGEN_MSG) ; \
		exit 1 ; \
	fi
	@if [ ! -e Makefile.common ] ; then \
		echo -e $(CONFIGURE_MSG) ; \
		exit 1 ; \
	fi
	$(MAKE) -C doc uninstall
	$(MAKE) -C utils/scripts uninstall
	$(MAKE) -C utils/eagleconnect uninstall
	$(MAKE) -C pppoa uninstall
	$(MAKE) -C driver uninstall
	#hash -r
	echo -e $(SEP_MSG)
	echo -e $(UNINSTALL_OK_MSG)
	if test $(BUILD_MODULE) == 1 ; then \
		echo -e $(UNINSTALL_OK2_MSG) ; \
	else \
		echo ; \
	fi

clean:
	-$(MAKE) -C driver clean
	-$(MAKE) -C pppoa clean
	-$(MAKE) -C utils/eagleconnect clean
	-$(MAKE) -C utils/scripts clean
#	-$(MAKE) -C doc clean

distclean: clean
	rm -f Makefile.common utils/eagleconfig/Makefile confdefs.h
	rm -rf config.{status,log,cache}

maintainer-clean: distclean
	rm -f configure aclocal.m4
	rm -Rf autom4te.cache
	$(MAKE) -C doc clean

dist: maintainer-clean
	rm -f ../$(ARCHNAME).tar.bz2 && \
	./makedist $(ARCHNAME) `pwd`

rpm: dist
	if [ `id -ur` == 0 ] ; then \
		rm -f /usr/src/RPM/SOURCES/$(ARCHNAME).tar.bz2 ; \
		cp ../eagle-usb.dist/$(ARCHNAME).tar.bz2 /usr/src/RPM/SOURCES ; \
		rpmbuild -ba eagle-usb.spec ; \
		mv -f /usr/src/RPM/RPMS/i586/eagle-usb-*.rpm ../eagle-usb.dist ; \
		mv -f /usr/src/RPM/SRPMS/eagle-usb-*.rpm ../eagle-usb.dist ; \
	else \
		if [ "`whereis -b rpmbuild`" = "rpmb:" ] ; then \
			echo "Error: rpm-build was not found on your system!" ; \
			exit 1 ; \
		fi ; \
		if [ ! -d ~/rpm ] ; then \
			echo "Error: ~/rpm missing, you should consider reading Mandrakesoft's RpmHowTo..." ; \
			exit 1 ; \
		fi ; \
		if [ ! -f ~/.rpmmacros ] || [ ! -f ~/.rpmrc ] ; then \
			echo "Error: ~/.rpmmacros or ~/.rpmrc is missing, you should consider reading Mandrakesoft's RpmHowTo..." ; \
			exit 1 ; \
		fi ; \
		rm -f ~/rpm/SOURCES/$(ARCHNAME).tar.bz2 ; \
		cp ../eagle-usb.dist/$(ARCHNAME).tar.bz2 ~/rpm/SOURCES ; \
		rpmbuild -ba eagle-usb.spec ; \
	fi
	#mv -f ~/rpm/RPMS/i586/eagle-usb-*.rpm ../eagle-usb.dist ; \
	#mv -f ~/rpm/SRPMS/eagle-usb-*.rpm ../eagle-usb.dist ; \
	#rpmbuild -ta ../eagle-usb.dist/$(ARCHNAME).tar.bz2
	#rpmbuild --define='_prefix /usr' -ba eagle-usb.spec

.PHONY: deb debclean
DEBVERSION = $(shell sed -ne '1s/.*(\(.*\)).*/\1/p' debian/changelog)
debclean:
	fakeroot debian/rules clean
	-$(RM) -R ../eagle-usb-${VERSION} ../eagle-usb-${VERSION}.orig
	-$(RM) ../eagle-usb_${VERSION}.orig.tar.gz
	-$(RM) ../eagle-usb*_${VERSION}-*.{diff.gz,dsc,deb,changes}
deb:
	# Clean the debian directory
	fakeroot debian/rules clean
	# Recopy all the source directory
	# but CVS and temporary files, and packaging files
	[ -d ../eagle-usb-${VERSION} ] || cp -r . ../eagle-usb-${VERSION}
	find ../eagle-usb-${VERSION} -name "CVS" -exec rm -rf {} \; -prune
	find ../eagle-usb-${VERSION} -name "*~" -exec rm -f {} \;
	find ../eagle-usb-${VERSION} -name "*.old" -exec rm -f {} \;
	rm -rf ../eagle-usb-${VERSION}/{debian,eagle-usb.spec}

	# Create original tarball file
	if [ ! -f ../eagle-usb-${VERSION}.orig.tar.gz ]; then \
		cd .. ; tar czf eagle-usb_${VERSION}.orig.tar.gz eagle-usb-${VERSION} ;\
	fi

	# Recopy packaging files specific to Debian
	cp -r debian/ ../eagle-usb-${VERSION}

	# Create diff and dsc files
	cd .. ; dpkg-source -b -sK "eagle-usb-${VERSION}" "eagle-usb_${VERSION}.orig.tar.gz"
	rm -rf ../eagle-usb-${VERSION}.orig

	# Build the three packages
	../eagle-usb-${VERSION}/debian/rules build
	fakeroot ../eagle-usb-${VERSION}/debian/rules binary

## Unquote the above lines if you want install the Debian packages
## into an APT repository.
	#lintian ../eagle-usb-*_${DEBVERSION}_*.deb
	#lintian ../eagle-usb_${DEBVERSION}.dsc
	#arch=`dpkg-architecture -qDEB_HOST_ARCH` ; \
	#dpkg-genchanges >../eagle-usb_${DEBVERSION}_$${arch}.changes

.PHONY: all build install uninstall clean distclean maintainer-clean
.PHONY: dist rpm

#***************************************************************************
# $Log: Makefile,v $
# Revision 1.38  2005/01/30 00:58:56  Tux
# - moved "depmod"
#
# Revision 1.37  2005/01/16 21:59:55  Tux
# - add license header
#
# Revision 1.36  2005/01/12 21:49:59  Tux
# - previous tests "@[ condition ] && result" were buggy => use "if [] ; then ; fi" structure instead
# - don't try to install documentation if it doesn't exist
# - add "depmod"
#
# Revision 1.35  2005/01/04 21:14:59  Tux
# - "hash -r" doesn't work in Makefile
#
# Revision 1.34  2005/01/02 23:09:10  mcoolive
# - improve the deb rule
#
# Revision 1.33  2004/11/30 19:27:09  mcoolive
# - check whether PATH contains $prefix/sbin at the end of install
# - cosmetic changes
#
# Revision 1.32  2004/11/27 18:14:43  mcoolive
# - remove useless ``&&''
#
# Revision 1.31  2004/11/27 14:27:29  mcoolive
# - test the value of GEN_DOC here
# - remove useless ``&&'' ( ``make -n'' works only if make is the first command)
#
# Revision 1.30  2004/11/17 20:54:29  Tux
# - don't move *.rpm to eagle-usb.dist
#
# Revision 1.29  2004/11/09 14:31:29  mcoolive
# - add VERSION variable, modify semantic of DEBVERSION variable
# - improve calculation of ARCHNAME
# - in deb rule: replace DEBVERSION by VERSION and pversion by DEBVERSION
#
# Revision 1.28  2004/11/04 17:07:13  mcoolive
# - fix "deb" target
#
# Revision 1.27  2004/10/31 23:59:14  mcoolive
# - fix the last modifications (rename orig.tar.gz file and clean the build)
#
# Revision 1.26  2004/10/31 20:04:12  mcoolive
# - improve the debian targets
#
# Revision 1.25  2004/10/18 21:31:01  Tux
# - as /usr is the default path, simplify rpmbuild command
#
# Revision 1.24  2004/10/10 13:48:14  Tux
# - removed unused string
#
# Revision 1.23  2004/10/04 20:45:09  Tux
# - "make rpm" now check if .rpmrc and .rpmmacros are present
#
# Revision 1.22  2004/09/05 20:34:20  mcoolive
# Improve the deb rule
#
# Revision 1.21  2004/08/26 21:36:41  Tux
# - "make clean" does not remove manpages anymore
#
# Revision 1.20  2004/08/25 13:55:25  mcoolive
# - fix deb rule: there was a bug when the name of the directory was
#   "eagle-usb-version" instead of "eagleusb" (tarball versus cvs)
#
# Revision 1.19  2004/08/16 21:42:39  Tux
# - rpm generation : use /usr as default prefix (instead of /usr/local)
#
# Revision 1.18  2004/08/12 22:27:12  mcoolive
# - fix deb rule : fix the generation of the files `changes'
#
# Revision 1.17  2004/08/09 21:28:45  mcoolive
# - correct the generation of *.orig.tar.gz (by default tar dump symlinks)
#
# Revision 1.16  2004/08/07 12:31:51  Tux
# - add rules to generate docs
#
# Revision 1.15  2004/08/06 09:59:10  mcoolive
# - edit "deb" rule to remove warnings
#
# Revision 1.14  2004/08/05 15:36:31  mcoolive
# - the "clean" rule is propagated in "doc" subdirectory
# - add rule "deb" to construct debian packages
#
# Revision 1.13  2004/08/01 20:08:21  Tux
# - eagleconnect support
#
# Revision 1.12  2004/05/30 01:50:04  Tux
# - "rpm -ba" => "rpmbuild"
#
# Revision 1.11  2004/05/11 19:42:49  Tux
# - eagle.spec => eagle-usb.spec
# - add "hash -r" after "make install"
#
# Revision 1.10  2004/04/20 19:55:07  Tux
# - cosmetic changes
#
# Revision 1.9  2004/04/18 19:46:38  Tux
# - a string was badly displayed
#
# Revision 1.8  2004/04/18 13:55:03  Tux
# *** empty log message ***
#
# Revision 1.7  2004/04/03 16:09:07  Tux
# - ask the user to unplug&replug modem
#
#***************************************************************************/
