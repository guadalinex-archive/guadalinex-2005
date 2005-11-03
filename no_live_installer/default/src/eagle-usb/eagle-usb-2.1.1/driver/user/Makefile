#------------------------------------------
# Makefile
#
# Makefile for eaglectrl
#

-include ../../Makefile.common

VERSION=$(shell cat ../../VERSION)


CFLAGS=-O2 -pipe -Wall -pedantic -DLINUX -g -DCONF_DIR="\"$(EU_DIR)\"" -DBIN_DIR="\"$(EU_DSP_DIR)\"" '-DEAGLEUSBVERSION="$(VERSION)"'

ifeq ($(USE_CMVS),1)
CFLAGS+=-DUSE_CMVS
endif

all: eaglectrl

eaglectrl: eaglectrl.c ../eagle-usb.h
	$(CC) $(CFLAGS) $< -o $@

install:
	$(INSTALL) -d $(SBINDIR) && \
	$(INSTALL) -d $(EU_DIR) && \
	$(INSTALL) -m 0755 eaglectrl $(SBINDIR) && \
	$(INSTALL) -m 0755 eaglestat $(SBINDIR) && \
	$(INSTALL) -m 0644 eagle-usb.conf $(EU_DIR)/eagle-usb.conf.template && \
	$(INSTALL) -m 0644 CMVei.txt $(EU_DIR) && \
	$(INSTALL) -m 0644 CMVeiWO.txt $(EU_DIR) && \
	$(INSTALL) -m 0644 CMVepWO.txt $(EU_DIR) && \
	$(INSTALL) -m 0644 CMVepES.txt $(EU_DIR) && \
	$(INSTALL) -m 0644 CMVepFR.txt $(EU_DIR) && \
	$(INSTALL) -m 0644 CMVepIT.txt $(EU_DIR) && \
	$(INSTALL) -m 0644 CMVep.txt $(EU_DIR) 

uninstall:
	rm -f $(SBINDIR)/eaglectrl
	rm -f $(SBINDIR)/eaglestat
	rm -f $(EU_DIR)/eagle-usb.conf.template
	rm -f $(EU_DIR)/CMVei.txt
	rm -f $(EU_DIR)/CMVepES.txt
	rm -f $(EU_DIR)/CMVepFR.txt
	rm -f $(EU_DIR)/CMVepIT.txt
	rm -f $(EU_DIR)/CMVep.txt

clean:
	rm -f eaglectrl

.PHONY: all install uninstall clean
