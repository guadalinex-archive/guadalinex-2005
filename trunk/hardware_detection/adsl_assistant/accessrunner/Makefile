ifndef KERNEL
KERNEL=$(shell uname -r)
endif

VERSION=$(shell echo ${KERNEL}|sed "s/^\([0-9]*\.[0-9]*\).*/\1/")

binutils:
	@make -C utils

installbins:
	install -c -m 755 utils/cxacru-fw ${PREFIX}/bin/cxacru-fw

modules: all

all:
	(cd drivers/usb/atm; make)

clean:
	@make -C drivers/usb/atm clean
	@make -C utils clean

install:
	@make -C drivers/usb/atm install

uninstall:
	@make -C drivers/usb/atm uninstall

.PHONY : all clean install uninstall binutils installbins installmod
