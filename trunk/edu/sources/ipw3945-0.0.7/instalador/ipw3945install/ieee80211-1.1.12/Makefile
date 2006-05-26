#
# Makefile for the ieee80211 kernel subsystem
# 
# Original makefile by Peter Johanson (from ipw2100)
#
# NOTE: This make file can serve as both an external Makefile (launched
#       directly by the user), or as the sub-dir Makefile used by the kernel
# 	build system.
#
# We assume that this build supercedes any in-kernel configurations.
EXTERNAL_BUILD=y
CONFIG_IEEE80211=m
CONFIG_IEEE80211_DEBUG=y

# We have to add drivers/net/wireless until ieee802_11.h is in the default
# include path
ifneq ($(CONFIG_IEEE80211_DEBUG),)
	EXTRA_CFLAGS += -g -Wa,-adhlms=$@.lst
endif

ifdef KBUILD_EXTMOD
	EXTRA_CFLAGS += -I$(KBUILD_EXTMOD)
endif

ifeq ($(IEEE80211_INC),)
        IEEE80211_INC := /lib/modules/$(shell uname -r)/include
endif

list-m :=

# If you do not want any encryption/decryption code in the driver, comment 
# out this line:
CONFIG_IEEE80211_CRYPT=$(CONFIG_IEEE80211)

# If you want to build WPA support, uncomment this line:
CONFIG_IEEE80211_CRYPT_WEP=$(CONFIG_IEEE80211)
CONFIG_IEEE80211_CRYPT_TKIP=$(CONFIG_IEEE80211)
CONFIG_IEEE80211_CRYPT_CCMP=$(CONFIG_IEEE80211)

list-$(CONFIG_IEEE80211) += ieee80211 
list-$(CONFIG_IEEE80211) += ieee80211_crypt
list-$(CONFIG_IEEE80211_CRYPT_WEP) += ieee80211_crypt_wep
list-$(CONFIG_IEEE80211_CRYPT_CCMP) += ieee80211_crypt_ccmp
list-$(CONFIG_IEEE80211_CRYPT_TKIP) += ieee80211_crypt_tkip

obj-$(CONFIG_IEEE80211) += ieee80211.o ieee80211_crypt.o
obj-$(CONFIG_IEEE80211_CRYPT_WEP) += ieee80211_crypt_wep.o
obj-$(CONFIG_IEEE80211_CRYPT_CCMP) += ieee80211_crypt_ccmp.o
obj-$(CONFIG_IEEE80211_CRYPT_TKIP) += ieee80211_crypt_tkip.o
ieee80211-objs := \
	ieee80211_module.o \
	ieee80211_tx.o \
	ieee80211_rx.o \
	ieee80211_wx.o \
	ieee80211_geo.o

ieee80211_inc := ieee80211.h ieee80211_crypt.h ieee80211_radiotap.h

#
# Begin dual Makefile mode here.  First we provide support for when we
# are being invoked by the kernel build system
#
ifneq ($(KERNELRELEASE),)

#
# If you receive a compile message about multiple definitions of
# CONFIG_IEEE80211_DEBUG, then you have IEEE80211 into the full 
# kernel build, and these definitions are now being set up by the kernel 
# build system.  
#
# To correct this, remove any CONFIG_IEEE80211... entries from 
# $(KSRC)/.config and $(KSRC)/include/linux/autoconf.h
# 

ifeq ($(EXTERNAL_BUILD),y)
ifdef CONFIG_IEEE80211_DEBUG
EXTRA_CFLAGS += -DCONFIG_IEEE80211_DEBUG=$(CONFIG_IEEE80211_DEBUG)
endif
ifdef CONFIG_IEEE80211_CRYPT_WEP
EXTRA_CFLAGS += -DCONFIG_IEEE80211_CRYPT_WEP=$(CONFIG_IEEE80211_CRYPT_WEP)
endif
ifdef CONFIG_IEEE80211_CRYPT_TKIP
EXTRA_CFLAGS += -DCONFIG_IEEE80211_CRYPT_TKIP=$(CONFIG_IEEE80211_CRYPT_TKIP)
endif
ifdef CONFIG_IEEE80211_CRYPT_CCMP
EXTRA_CFLAGS += -DCONFIG_IEEE80211_CRYPT_CCMP=$(CONFIG_IEEE80211_CRYPT_CCMP)
endif
endif

else 
# Here we begin the portion that is executed if the user invoked this Makefile
# directly.

# To make this as easy on the user as possible, we first check for any
# pre-existing in-kernel definitions of ieee80211 components that might
# conflict with this build system...

# KSRC should be set to the path to your sources
# modules are installed into KMISC
KVER  := $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
KMISC := /lib/modules/$(KVER)/net/ieee80211/
KMISC_INC := /lib/modules/$(KVER)/build/include/net/

# KSRC_OUTPUT should be overridden if you are using a 2.6 kernel that
# has it's output sent elsewhere via KBUILD_OUTPUT= or O=
KSRC_OUTPUT := $(KSRC)

# If we find Rules.make, we can assume we're using the old 2.4 style building
OLDMAKE=$(wildcard $(KSRC)/Rules.make)
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

all: check_old modules

check_old:
	@$(SHELL) $(PWD)/remove-old $(KSRC) || \
		(echo "Old ieee80211 references found.  In order to build the ieee80211"; \
		echo "subsystem, prior versions must first be removed.  You can perform"; \
		echo "this task by running this makefile as root via:"; \
		echo; \
		echo "    % sudo make check_old"; \
		echo; \
		echo "and answering Y to remove the file references."; \
		echo "Aborting make." && \
			exit 1)

clean:
	rm -f *.mod.c *.mod *.o *.ko .*.cmd .*.flags *.lst *~ .#*
	rm -rf $(PWD)/tmp .tmp_versions
	for file in *.{c,h} net/*.h; do \
		if [ -e $$file ]; then \
			sed -i -e "s:\ *$$::g" -e "s:\t*$$::g" $$file; \
		fi \
	done

TMP=$(PWD)/tmp
MODVERDIR=$(TMP)/.tmp_versions

modules:
ifdef ($(KSRC_OUTPUT)/.tmp_versions)
	mkdir -p $(MODVERDIR)
	-cp $(KSRC_OUTPUT)/.tmp_versions/*.mod $(MODVERDIR)
endif
ifeq ($(KSRC),$(KSRC_OUTPUT)) # We're not outputting elsewhere
ifdef ($(KSRC)/.tmp_versions)
	-cp $(KSRC)/.tmp_versions/*.mod $(MODVERDIR)
endif
	$(MAKE) -C $(KSRC) M=$(PWD) MODVERDIR=$(PWD) modules
else # We've got a kernel with seperate output, copy the config, and use O=
	mkdir -p $(TMP)
	cp $(KSRC_OUTPUT)/.config $(TMP)
	$(MAKE) -C $(KSRC) M=$(PWD) MODVERDIR=$(PWD) O=$(PWD)/tmp modules
endif

check_path:
	@if [ "$(IEEE80211_INC)" = "" ]; then \
		echo -e \
"\n You need to provide the IEEE80211 target include path when running\n"\
"make install.  For example:\n\n"\
"\t% make IEEE80211_INC=/usr/include install\n\n"\
"will install the header files into /usr/include/net.\n"; \
		exit 1; \
	fi

override IEEE80211_INC := \
	`echo $(IEEE80211_INC) | grep "/net\$$" || \
		echo $(IEEE80211_INC)/net`

patch_kernel:
	@echo -e \
"\n This will install this IEEE80211 subsystem into your\n"\
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
	@[ -e ${KSRC}/net/ieee80211 ] || \
		mkdir ${KSRC}/net/ieee80211
	@cp {*.c,in-tree/{Makefile,Kconfig}} \
		${KSRC}/net/ieee80211/
	@cp net/*.h ${KSRC}/include/net/
	@(grep -q "ieee80211" ${KSRC}/net/Makefile || \
		echo \
"obj-\$$(CONFIG_IEEE80211)         += ieee80211/" >> \
		${KSRC}/net/Makefile)
	@(grep -q "ieee80211" ${KSRC}/net/Kconfig || \
		sed -ie "s:^\(endif.*# if NET\):source \"net/ieee80211/Kconfig\"\n\n\1:g" \
		${KSRC}/net/Kconfig)
	@echo -e "Kernel has been udpated to this IEEE80211 subsystem.\n"

install: check_path modules
	install -d $(KMISC)
	install -m 644 -c $(addsuffix .ko,$(list-m)) $(KMISC)
	install -d $(IEEE80211_INC)
	install -m 644 -c $(addprefix net/,$(ieee80211_inc)) $(IEEE80211_INC)
	-mkdir -p $(KMISC)/.tmp_versions
	install -m 644 -c $(addsuffix .mod,$(list-m)) $(KMISC)/.tmp_versions
	/sbin/depmod -a ${KVER}

uninstall:
	rm -rf $(addprefix $(KMISC),$(addsuffix .ko,$(list-m)))
	rm -rf $(addprefix $(KMISC_INC), $(ieee80211_inc))
	/sbin/depmod -a ${KVER}

endif # End of internal build

.PHONY: TAGS tags check_old check_path all

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

