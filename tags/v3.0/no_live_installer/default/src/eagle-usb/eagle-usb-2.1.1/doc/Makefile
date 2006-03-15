# $Id: Makefile,v 1.10 2004/11/30 18:46:50 mcoolive Exp $

-include ../Makefile.common

SOURCES		:= $(filter %.xml, $(shell ls sources))
MAN_PAGES	:= $(addprefix man/, ${SOURCES:.xml=.8})
HTML_PAGES	:= $(addprefix html/, ${SOURCES:.xml=.html})

.PHONY: all
all: man

.PHONY: man
man: ${MAN_PAGES}
${MAN_PAGES}: man/%.8: sources/%.xml
#	xmlto man -o man $<
	xsltproc --nonet -o $@ ${DOCBOOK_STYLESHEETS}/manpages/docbook.xsl $<

.PHONY: html
html: ${HTML_PAGES}
${HTML_PAGES}: html/%.html: sources/%.xml
#	xmlto html -o html $<
	xsltproc --nonet -o $@ ${DOCBOOK_STYLESHEETS}/html/docbook.xsl $<

.PHONY: install
install: man
	$(INSTALL) -d ${MANDIR}/man8
	cp ${MAN_PAGES} ${MANDIR}/man8

.PHONY: uninstall
uninstall:
	rm -f $(addprefix ${MANDIR}/man8/, ${MAN_PAGES})

.PHONY: clean
clean:
	rm -rf man html

.PHONY: distclean
distclean: clean

#***************************************************************************
# $Log: Makefile,v $
# Revision 1.10  2004/11/30 18:46:50  mcoolive
# - About documentation generation: don't use xmlto,
#   use xlstproc (as before but it is simpler).
#
# Revision 1.9  2004/11/27 14:24:42  mcoolive
# - use xmlto instead of xsltproc. It is simpler.
#
# Revision 1.8  2004/11/17 21:01:56  Tux
# - MAN_PAGE and HTML_PAGE should always be declared because there are used by the "make clean" rule
#
# Revision 1.7  2004/08/23 09:01:54  mcoolive
# - --nonet option to force the use of local entities
#
# Revision 1.6  2004/08/09 22:08:34  mcoolive
# - add definitions of install and uninstall rules
# - if (GEN_DOC != yes) we do anything
#
# Revision 1.4  2004/08/07 12:33:42  Tux
# - removed stylesheets paths (moved to Makefile.common)
#
# Revision 1.3  2004/08/05 15:44:47  mcoolive
# - addition of a filter in the calculation of SOURCES
#   to be unaware of sub-directory "CVS".
#
# Revision 1.2   2004/08/03 mcoolive
# - Fix (source => sources). 
#
# Revision 1.1   2004/08/03 mcoolive
# - initial release
#
#***************************************************************************/
