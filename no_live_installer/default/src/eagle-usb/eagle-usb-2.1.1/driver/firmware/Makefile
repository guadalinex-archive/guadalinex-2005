# $Id: Makefile,v 1.7 2004/12/30 16:39:36 mcoolive Exp $
#------------------------------------------
# Makefile for buildfirmware and buildDSP
#
# Build and executes tools to generate binary
# firmware from ADI's headers and .bnm files.
#
# ADI's firmware header is not free software,
# buildfirmware is therefore in public domain.
# .bnm files are not open sourced, but can be
# considered as data files for buildDSP, and
# buildDSP is published under the GNU GPL license.

-include ../../Makefile.common

CFLAGS=-O2 -pipe -Wall -pedantic

POTS_DIR=sagem/pots
ISDN_DIR=sagem/isdn
POTS_FILES=$(wildcard $(POTS_DIR)/*.bnm)
ISDN_FILES=$(wildcard $(ISDN_DIR)/*.bnm)

all: dsp_code_pots.bin dsp_code_isdn.bin

dsp_code_pots.bin: buildDSP $(POTS_FILES)
	./buildDSP -d $(POTS_DIR) $@

dsp_code_isdn.bin: buildDSP $(ISDN_FILES)
	./buildDSP -d $(ISDN_DIR) $@

buildDSP: builddsp.c
	$(CC) $(CFLAGS) builddsp.c -o buildDSP

install:
	$(INSTALL) -d $(EU_DSP_DIR) && \
	$(INSTALL) -m 0664 dsp_code_pots.bin $(EU_DSP_DIR)
	$(INSTALL) -m 0664 dsp_code_isdn.bin $(EU_DSP_DIR)

uninstall:
	rm -f $(EU_DSP_DIR)/dsp_code_pots.bin $(EU_DSP_DIR)/dsp_code_isdn.bin
	# there may be better solution to do this :-/
	if test -d $(EU_DSP_DIR) && test -z "`ls $(EU_DSP_DIR)`" ; then \
		rm -Rf $(EU_DSP_DIR) ; \
	fi

clean:
	rm -f buildDSP dsp_code_isdn.bin dsp_code_pots.bin

.PHONY: all install uninstall clean
