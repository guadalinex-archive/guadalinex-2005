TARGET=usr/share/watermain

install: logo.png watermain.glade watermain
	(if [ ! -d $(DESTDIR)/$(TARGET) ]; then mkdir $(DESTDIR)/$(TARGET); fi;) && \
	install -m 444 logo.png $(DESTDIR)/$(TARGET) && \
	install -m 444 watermain.glade $(DESTDIR)/$(TARGET) && \
	install -m 544 watermain $(DESTDIR)/usr/sbin

uninstall: 
	rm -Rf $(DESTDIR)/$(TARGET) $(DESTDIR)/usr/bin/watermain
