#
# Makefile for the Linux Wireless network device drivers.
#
# Original makefile by Peter Johanson
#
# NOTE: This make file can serve as both an external Makefile (launched
#       directly by the user), or as the sub-dir Makefile used by the kernel
# 	build system.

# If CONFIG_IPW* isn't set, we'll assume the user has never configured
# their kernel to include this module and set up some defaults.
#
# NOTE: If you have previously added the IPW project to your kernel 
# 	and configured it for inclusion, these settings will be 
#	overridden by your kernel configuration.
ifndef CONFIG_IPW3945
EXTERNAL_BUILD=y
CONFIG_IPW3945=m
CONFIG_IPW3945_DEBUG=y
#CONFIG_IPW3945_QOS=y

# If you are not interested in using monitor mode, simply comment out:
#
# NOTE:  If you have problems compiling due to IW_MODE_MONITOR not being
#        defined then you need to update the wireless extension version
#	 installed in your kernel, or comment this line out.
#
# -- THIS IS CURRENTLY *VERY* LIMITED --
# CONFIG_IPW3945_MONITOR=y

# If you are interested in using radiotap headers in monitor mode,
# simply uncomment:
#CONFIG_IEEE80211_RADIOTAP=y

endif

# We have to add drivers/net/wireless until ieee802_11.h is in the default
# include path
EXTRA_CFLAGS += -I$(TOPDIR)/drivers/net/wireless 

ifeq ($(IEEE80211_INC),)
	IEEE80211_INC := /lib/modules/$(shell uname -r)/include
	IEEE80211_MODVERDIR=/lib/modules/$(KVER)/net/ieee80211/.tmp_versions
else
	IEEE80211_MODVERDIR=$(IEEE80211_INC)
endif
EXTRA_CFLAGS += -I$(IEEE80211_INC)

ifeq ($(CONFIG_IPW3945_DEBUG),y)
	EXTRA_CFLAGS += -g -Wa,-adhlms=$@.lst
endif

ifdef KBUILD_EXTMOD
	EXTRA_CFLAGS += -I$(KBUILD_EXTMOD)
endif

list-m :=
list-$(CONFIG_IPW3945) += ipw3945
obj-$(CONFIG_IPW3945) += ipw3945.o

#
# Begin dual Makefile mode here.  First we provide support for when we
# are being invoked by the kernel build system
#
ifneq ($(KERNELRELEASE),)

#
# If you receive a compile message about multiple definitions of
# CONFIG_IPW3945_DEBUG, then you have IPW* into the full kernel build, and 
# these definitions are now being set up by the kernel build system.  
#
#
# To correct this, remove any CONFIG_{IPW3945,IEEE80211}* lines from 
# $(KSRC)/.config and $(KSRC)/include/linux/autoconf.h
# 

EXTRA_CFLAGS += -DCONFIG_PM

ifeq ($(EXTERNAL_BUILD),y)
ifdef CONFIG_IPW3945_DEBUG	
EXTRA_CFLAGS += -DCONFIG_IPW3945_DEBUG=$(CONFIG_IPW3945_DEBUG)
endif
ifdef CONFIG_IPW3945_MONITOR
EXTRA_CFLAGS += -DCONFIG_IPW3945_MONITOR=$(CONFIG_IPW3945_MONITOR)
ifdef CONFIG_IEEE80211_RADIOTAP
EXTRA_CFLAGS += -DCONFIG_IEEE80211_RADIOTAP=$(CONFIG_IEEE80211_RADIOTAP)
endif
endif
ifdef CONFIG_IPW_QOS
EXTRA_CFLAGS += -DCONFIG_IPW_QOS=$(CONFIG_IPW_QOS)
endif
endif

else 
# Here we begin the portion that is executed if the user invoked this Makefile
# directly.

# KSRC should be set to the path to your sources
# modules are installed into KMISC
KVER  := $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
KMISC := $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/net/wireless/
KMISC_INC := $(DESTDIR)/lib/modules/$(KVER)/include

# KSRC_OUTPUT should be overridden if you are using a 2.6 kernel that
# has it's output sent elsewhere via KBUILD_OUTPUT= or O=
KSRC_OUTPUT := $(KSRC)

PWD=$(shell pwd)

VERFILE := $(KSRC_OUTPUT)/include/linux/version.h
KERNELRELEASE := $(shell \
	if [ -r $(VERFILE) ]; then \
		(cat $(VERFILE); echo UTS_RELEASE) | \
		$(CC) -I$(KSRC_OUTPUT) $(CFLAGS) -E - | \
		tail -n 1 | \
		xargs echo; \
        else \
		uname -r; \
	fi)

MODPATH := $(DESTDIR)/lib/modules/$(KERNELRELEASE)

all: check_inc modules

check_inc:
	@([ ! -e $(IEEE80211_INC)/net/ieee80211.h ] && \
	echo -e \
"\n ERROR: ieee80211.h not found in '$(IEEE80211_INC)'.\n\n"\
"You need to install the ieee80211 subsystem from http://ieee80211.sf.net\n"\
"and point this build to the location where you installed those sources, eg.:\n\n"\
"% make IEEE80211_INC=/usr/src/ieee80211/\n\n"\
"will look for ieee80211.h in /usr/src/ieee80211/net/\n") || exit 0
	@[ -e $(IEEE80211_INC)/net/ieee80211.h ]

clean:
	rm -f *.mod.c *.mod *.o *.ko .*.cmd .*.flags .lst *.lst
	rm -rf $(PWD)/tmp
	for file in *.{c,h}; do \
		sed -i -e "s:\ *$$::g" -e "s:\t*$$::g" $$file; \
	done

distclean: clean
	rm -f tags TAGS

TMP=$(PWD)/tmp
MODVERDIR=$(TMP)/.tmp_versions

modules:
	mkdir -p $(MODVERDIR)
	-cp $(IEEE80211_MODVERDIR)/*.mod $(MODVERDIR)
ifdef ($(KSRC_OUTPUT)/.tmp_versions)
	-cp $(KSRC_OUTPUT)/.tmp_versions/*.mod $(MODVERDIR)
endif
ifeq ($(KSRC),$(KSRC_OUTPUT)) # We're not outputting elsewhere
ifdef ($(KSRC)/.tmp_versions)
	-cp $(KSRC)/.tmp_versions/*.mod $(MODVERDIR)
endif
	$(MAKE) -C $(KSRC) M=$(PWD) MODVERDIR=$(MODVERDIR) modules
else # We've got a kernel with seperate output, copy the config, and use O=
	mkdir -p $(TMP)
	cp $(KSRC_OUTPUT)/.config $(TMP)
	$(MAKE) -C $(KSRC) M=$(PWD) MODVERDIR=$(MODVERDIR)) O=$(PWD)/tmp modules
endif



patch_kernel:
	@echo -e \
"\n This will install this IPW3945 driver into your\n"\
" kernel tree located here:\n"\
"\n${KSRC}\n\n"\
" If you would like to instal to a different location, run\n"\
" this as follows: make KSRC=/path/to/kernel patch_kernel \n"
	@(read -p "Do you wish to continue? [Yn] " reply; \
	case $$reply in \
		Y|y|"") exit 0 ;; \
		*) echo "Terminating patch prcoess." ; exit 1 ;; \
	esac)
	@if [ "$(shell whoami)" != "root" ]; then \
	    echo -e \
"\nIf this fails, you may need to be root to patch the kernel.\n" ; \
	fi
	@(grep -q "ipw3945" ${KSRC}/drivers/net/wireless/Makefile || \
		echo \
"obj-\$$(CONFIG_IPW3945)         += ipw3945.o" >> \
		${KSRC}/drivers/net/wireless/Makefile)
	@cp README.ipw3945 ${KSRC}/Documentation/networking
	@cp in-tree/Kconfig.ipw3945 ${KSRC}/drivers/net/wireless
	@(grep -q "Kconfig\.ipw3945" ${KSRC}/drivers/net/wireless/Kconfig || \
	grep -q "IPW3945" ${KSRC}/drivers/net/wireless/Kconfig || \
		sed -i -e "s:^endmenu:source \"drivers/net/wireless/Kconfig.ipw3945\"\n\nendmenu:g" ${KSRC}/drivers/net/wireless/Kconfig)
	@cp ipw3945.{c,h} ipw3945_daemon.h ${KSRC}/drivers/net/wireless/
	@echo -e "Kernel has been udpated to include this ipw3945 driver.\n"

install: modules binaries_install
	install -d $(KMISC)
	install -m 644 -c $(addsuffix .ko,$(list-m)) $(KMISC)

	/sbin/depmod -a ${KVER}
	@echo "Don't forget to copy microcode to your hotplug's firmware directory and have the "
	@echo "hotplug tools in place."
	@echo "See INSTALL for more information."

binaries_install:
	@echo "Instalando las utilidades"
	@echo "Desde $(PWD)/../sbin/* a $(DESTDIR)/sbin/..."
	install -d $(DESTDIR)/sbin/
	install -o root -g root -m 700 $(PWD)/../sbin/* $(DESTDIR)/sbin/


uninstall:
	rm -rf $(addprefix $(KMISC),$(addsuffix .ko,$(list-m)))
	/sbin/depmod -a ${KVER}

endif # End of internal build


.PHONY: TAGS tags check_inc

RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS \) -prune -o
define all-sources
	( find . $(RCS_FIND_IGNORE) -name '*.[chS]' -print )
endef

TAGS:
	$(all-sources) | etags -
tags:
	rm -f $@
	CTAGSF=`ctags --version | grep -i exuberant >/dev/null && echo "-I __initdata,__exitdata,EXPORT_SYMBOL,EXPORT_SYMBOL_NOVERS"`; \
	$(all-sources) | xargs ctags $$CTAGSF -a

