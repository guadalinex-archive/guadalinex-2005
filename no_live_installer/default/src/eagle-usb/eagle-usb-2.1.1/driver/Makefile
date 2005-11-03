# $Id: Makefile,v 1.5 2004/10/17 20:34:38 sleeper Exp $


OBJS = eu_main.o 	\
       eu_utils.o  	\
       Pipes.o 		\
       Me.o    		\
       Sm.o    		\
       eu_msg.o   	\
       Dsp.o   		\
       Mpoa.o  		\
       Uni.o   		\
       Sar.o   		\
       Oam.o   		\
       eu_eth.o         \
       eu_boot_sm.o


ifneq ($(KERNELRELEASE),)

EXTRA_CFLAGS+= '-DEAGLEUSBVERSION="$(VERSION)"'

#
# Enable BULK USB transfers
#

ifdef BULK
EXTRA_CFLAGS += -DUSEBULK
endif

ifeq ($(USE_CMVS),1)
EXTRA_CFLAGS+=-DUSE_CMVS
endif


obj-m := eagle-usb.o
eagle-usb-objs := $(OBJS)

else

-include ../Makefile.common

ifeq ($(BUILD_MODULE), 0)
include Makefile-module_disabled
else
ifneq (,$(findstring 2.6,$(KERNELVER)))

#
# Kernel 2.6
#
include Makefile-2.6

else

#
# Kernel 2.4
#
include Makefile-2.4

endif
endif
endif
