SUBDIRS = branding data

INCLUDES =                                      			\
	-DDATADIR=\"$(datadir)\"                			\
	-I$(top_srcdir)                         			\
	-I$(top_builddir)                       			\
	-DUIDIR=\""$(datadir)/uda-postinstall/ui/"\"			\
        -DLISTSDIR=\""$(listsdir)/uda-postinstall/lists/"\"		\
	-DBRANDINGDIR=\""$(datadir)/uda-postinstall/branding/"\"        \
	$(UDA_POSTINSTALL_CFLAGS)


backend_DATA = uda-postconfig-backend.sh detect-additional-partitions.py repositorio*
backenddir = $(datadir)/uda-postinstall/backend/

bin_PROGRAMS = 	uda-postinstall uda-postconfig uda-goodbye 

uda_postinstall_SOURCES = uda-postinstall.c
uda_postinstall_LDADD = $(UDA_POSTINSTALL_LIBS)

uda_postconfig_SOURCES = uda-postconfig.c
uda_postconfig_LDADD = $(UDA_POSTINSTALL_LIBS)

uda_goodbye_SOURCES = uda-goodbye.c
uda_goodbye_LDADD = $(UDA_POSTINSTALL_LIBS)

EXTRA_DIST = $(backend_DATA)
