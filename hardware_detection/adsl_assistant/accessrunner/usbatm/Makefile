ifndef KERNELDIR
	KERNELDIR  := /lib/modules/$(shell uname -r)/build
endif

FIRMWARE_EXTRACTOR := speedtch-fw

obj-m := cxacru.o speedtch.o usbatm.o xusbatm.o

all help modules modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(filter-out all,$(MAKECMDGOALS)) EXTRA_CFLAGS=-DDEBUG

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

clean-fw:
	-rm $(FIRMWARE_EXTRACTOR)

nodebug:
	make -C $(KERNELDIR) M=$(PWD)

firmware: firmware.c firmware.h
	gcc -o $(FIRMWARE_EXTRACTOR) firmware.c -DSTANDALONE_EXTRACTER

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
	/sbin/depmod -ae
