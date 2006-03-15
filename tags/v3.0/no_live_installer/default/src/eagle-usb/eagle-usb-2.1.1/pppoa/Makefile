#***********************************************************************
#
# Makefile
#
# Modified and enhanced by Anoosh Naderi from ADI on May22,2002 to
# support PPPoA
#
# This program may be distributed according to the terms of the GNU
# General Public License, version 2 or (at your option) any later version.
#
#***********************************************************************

-include ../Makefile.common

# Version is set ONLY IN THE MAKEFILE!  Don't delete this!
VERSION=$(shell cat ../VERSION)

# Program paths
#PPPOA_PATH=$(SBINDIR)/pppoa
#PATHS='-DPPPOA_PATH="$(PPPOA_PATH)"' '-DPPPD_PATH="$(PPPD_PATH)"'
CFLAGS= -O2 -Wall -Wstrict-prototypes -ansi -pedantic $(DEFINES) $(PATHS)

all: pppoa

pppoa: pppoa.o if.o debug.o common.o ppp.o
	$(CC) -o pppoa pppoa.o if.o debug.o common.o ppp.o

pppoa.o: pppoa.c pppoa.h
	$(CC) $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o pppoa.o pppoa.c

ppp.o: ppp.c pppoa.h
	$(CC) $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o ppp.o ppp.c

if.o: if.c pppoa.h
	$(CC) $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o if.o if.c

common.o: common.c pppoa.h
	$(CC) $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o common.o common.c

debug.o: debug.c pppoa.h
	$(CC) $(CFLAGS) '-DVERSION="$(VERSION)"' -c -o debug.o debug.c

install: pppoa
	$(INSTALL) -d $(SBINDIR)
	$(INSTALL) -m 755 pppoa $(SBINDIR)

uninstall:
	rm -f $(SBINDIR)/pppoa

clean:
	rm -f *.o pppoa 

.PHONY: all install uninstall clean
