POFILES=$(wildcard *.po)
MOFILES=$(POFILES:.po=.mo)
EXECUTABLES=$(shell find .. -perm +111 -type f)

all: transition-copies base-config.pot $(MOFILES)

transition-copies: no.mo
# Make sure no.po and nb.po are identical, while we change the install
# base to use nb_NO instead of no_NO.  Only nb.po should be
# translated, but no.mo should be installed. [pere 2003-12-20]
no.po: nb.po
	cp nb.po no.po

install: all
	for file in $(MOFILES); do \
		lang=`echo $$file | sed 's/\.mo//'`; \
		install -d $(prefix)/usr/share/locale/$$lang/LC_MESSAGES/; \
		install -m 0644 $$file $(prefix)/usr/share/locale/$$lang/LC_MESSAGES/base-config.mo; \
	done

base-config.pot: $(EXECUTABLES)
	@echo "Rebuilding the pot file"
	rm -f base-config.pot tmp.*.pot
	cnt=0;								\
	for file in $(EXECUTABLES); do			\
		cnt=$$(expr $$cnt + 1);					\
		bn=$$cnt.`basename $$file`;					\
		if head -n 1 $$file | grep -q bin/sh ; then		\
			xgettext -c -L Shell -o tmp.$$bn.pot $$file;	\
		elif head -n 1 $$file | grep -q perl ; then		\
			xgettext -c -L C -o tmp.$$bn.pot $$file;	\
		fi;							\
	done
	msgcat tmp.*.pot > base-config.pot
	rm -f tmp.*.pot

clean:
	rm -f $(MOFILES) messages messages.mo
	rm -f no.po # norwegian bokmal transition copy

%.mo: %.po
	msgfmt -o $@ $<

%.po: base-config.pot
	@echo -n "Merging base-config.pot and $@"
	@msgmerge $@ base-config.pot -o $@.new
# Typically all that changes was a date. I'd prefer not to commit such
# changes, so detect and ignore them.
	@if [ "`diff $@ $@.new | grep '[<>]' | wc -l`" -ne 2 ]; then \
		mv -f $@.new $@; \
	else \
		rm -f $@.new; \
	fi
	@msgfmt --statistics $@

check:
	@for file in $(POFILES); do \
		lang=`echo $$file | sed 's/\.po//'`; \
		printf "$$lang: "; \
		msgfmt -o /dev/null -c -v --statistics $$lang.po;\
	done
