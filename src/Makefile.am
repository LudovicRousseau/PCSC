SUBDIRS = spy

AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC -I$(top_builddir)/src/PCSC \
	$(SYMBOL_VISIBILITY)

lib_LTLIBRARIES = libpcsclite.la libpcsclite_real.la
check_LTLIBRARIES = libpcsclite_fake.la

sbin_PROGRAMS = pcscd
noinst_PROGRAMS = testpcsc pcsc-wirecheck pcsc-wirecheck-gen

if ENABLE_SERIAL
SERIAL_CONFIG = configfile.l
endif

if ENABLE_USB
USB_CONFIG = tokenparser.l \
	hotplug_libudev.c \
	hotplug_libusb.c \
	hotplug_macosx.c
endif

libpcsclite_la_SOURCES = \
	error.c \
	g_defines.c \
	libredirect.c \
	sys_unix.c
libpcsclite_la_LDFLAGS = -version-info 1:0:0

libpcsclite_real_la_SOURCES = \
	debug.c \
	winscard_clnt.c \
	simclist.c \
	sys_unix.c \
	utils.c \
	winscard_msg.c
libpcsclite_real_la_LDFLAGS = -version-info 1:0:0
libpcsclite_real_la_CFLAGS = $(PTHREAD_CFLAGS) -DLIBPCSCLITE -DSIMCLIST_NO_DUMPRESTORE
libpcsclite_real_la_LIBADD = $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

libpcsclite_fake_la_SOURCES = \
	libfake.c

pcscd_SOURCES = \
	auth.c \
	auth.h \
	atrhandler.c \
	atrhandler.h \
	configfile.h \
	$(SERIAL_CONFIG) \
	debuglog.c \
	dyn_generic.h \
	dyn_macosx.c \
	dyn_unix.c \
	eventhandler.c \
	eventhandler.h \
	hotplug_generic.c \
	hotplug.h \
	ifdwrapper.c \
	ifdwrapper.h \
	misc.h \
	parser.h \
	pcscdaemon.c \
	pcscd.h.in \
	PCSC/debuglog.h \
	PCSC/ifdhandler.h \
	PCSC/pcsclite.h.in \
	PCSC/winscard.h \
	PCSC/wintypes.h \
	prothandler.c \
	prothandler.h \
	readerfactory.c \
	readerfactory.h \
	simclist.c \
	simclist.h \
	sys_generic.h \
	sys_unix.c \
	$(USB_CONFIG) \
	utils.c \
	utils.h \
	winscard.c \
	winscard_msg.c \
	winscard_msg.h \
	winscard_msg_srv.c \
	winscard_svc.c \
	winscard_svc.h
pcscd_CFLAGS = $(PTHREAD_CFLAGS) $(LIBUSB_CFLAGS) $(LIBUDEV_CFLAGS) \
	$(POLKIT_CFLAGS) $(LIBSYSTEMD_CFLAGS) \
	-DPCSCD -DSIMCLIST_NO_DUMPRESTORE
pcscd_LDFLAGS = $(LDFLAGS) $(LIBSYSTEMD_LIBS) -export-dynamic
pcscd_LDADD = \
	$(PTHREAD_LIBS) $(COREFOUNDATION) \
	$(LIBUSB_LIBS) $(IOKIT) $(LIBUDEV_LIBS) \
	$(PTHREAD_LIBS) $(PTHREAD_CFLAGS) \
	$(POLKIT_LIBS) 

fix-rights: install-sbinPROGRAMS
	chgrp pcscd $(DESTDIR)$(sbindir)/pcscd
	chmod g+s $(DESTDIR)$(sbindir)/pcscd

testpcsc_SOURCES = testpcsc.c
testpcsc_LDADD = libpcsclite.la

pcsc_wirecheck_gen_SOURCES = \
	pcsc-wirecheck-gen.c

# pcsc_wirecheck_gen_CPPFLAGS := $(LIBPCSCLITE_CFLAGS)

pcsc_wirecheck_SOURCES = \
	lassert.h \
	pcsc-wirecheck-dist.c \
	pcsc-wirecheck-main.c

# pcsc_wirecheck_CFLAGS := $(LIBPCSCLITE_CFLAGS) -DCOMPAT_64BIT_SERVER=1

$(srcdir)/pcsc-wirecheck-dist.c: pcsc-wirecheck-gen
	@if test -f $@ ; then \
		echo "touch $@" ; \
		touch $@ ; \
	else \
		echo "rm -f $@" ; \
		rm -f $@ ; \
		echo "./pcsc-wirecheck-gen > $@.tmp" ; \
		./pcsc-wirecheck-gen > $@.tmp ; \
		echo "mv $@.tmp $@" ; \
		mv $@.tmp $@ ; \
	fi

.PHONY: regen

regen: pcsc-wirecheck-gen
	rm -f $(srcdir)/pcsc-wirecheck-dist.c
	./pcsc-wirecheck-gen > $(srcdir)/pcsc-wirecheck-dist.c.tmp
	mv $(srcdir)/pcsc-wirecheck-dist.c.tmp $(srcdir)/pcsc-wirecheck-dist.c

nobase_include_HEADERS = \
	PCSC/debuglog.h \
	PCSC/ifdhandler.h \
	PCSC/reader.h \
	PCSC/winscard.h \
	PCSC/wintypes.h

nodistheaderdir = $(includedir)/PCSC
nodistheader_DATA = PCSC/pcsclite.h

pcdir= $(libdir)/pkgconfig
pc_DATA = libpcsclite.pc

# Hack to be able to use flex -P to enable linking of multiple lexer
# sources into one library, until we find a way to make automake handle
# this automagically.  This breaks if lex is not flex!!
tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.tp.c $@ -- "$(LEX)" -Ptp $(AM_LFLAGS) $(LFLAGS)

EXTRA_DIST = README_INTERNALS.txt
