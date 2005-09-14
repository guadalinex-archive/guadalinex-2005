ifndef KERNEL
KERNEL=$(shell uname -r)
endif

VERSION=$(shell echo ${KERNEL}|sed "s/^\([0-9]*\.[0-9]*\).*/\1/")

binutils:
	@make -C usbatm firmware
	@make -C utils

installbins:
	install -c -m 755 utils/cxacru-fw ${PREFIX}/bin/cxacru-fw
	install -c -m 755 usbatm/speedtch-fw ${PREFIX}/bin/speedtch-fw

modules: all

all:
	(cd usbatm; make)

clean:
	@make -C usbatm clean

install:
	@make -C usbatm install

uninstall:
	@make -C usbatm uninstall

.PHONY : all clean install uninstall binutils installbins installmod
