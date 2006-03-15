ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m += cxacru.o
obj-m += usbatm.o

accessrunner-objs := cxacru.o usbatm.o

else
# external module build

ifndef KERNEL
KERNEL=$(shell uname -r)
endif

ifndef MODULESDIR
MODULESDIR=${ROOT}/lib/modules/${KERNEL}
endif

ifndef KERNEL_SOURCES
KERNEL_SOURCES=${MODULESDIR}/build
endif

LINVER := linux-${KERNEL}
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNEL_SOURCES) SUBDIRS=$(PWD) modules

clean: 
	rm -f *.o *.ko *.mod.c .*o.cmd

install: cxacru.ko usbatm.ko
	rm -f ${MODULESDIR}/kernel/misc/cxacru.ko
	rm -f ${MODULESDIR}/kernel/misc/usbatm.ko
	install -m644 -b -D cxacru.ko ${MODULESDIR}/kernel/misc/cxacru.ko
	install -m644 -b -D usbatm.ko ${MODULESDIR}/kernel/misc/usbatm.ko
	if [ -x /sbin/depmod -a "${ROOT}" = "/" ]; then /sbin/depmod -aq; fi

uninstall:
	rm -rf ${MODULESDIR}/kernel/misc/cxacru.ko
	rm -rf ${MODULESDIR}/kernel/misc/usbatm.ko
	if [ -x /sbin/depmod -a "${ROOT}" = "/" ]; then /sbin/depmod -aq; fi

endif

.PHONY : all tidy clean install uninstall
