## Process this file with automake to create Makefile.in.
# $Id$

pcdir= $(libdir)/pkgconfig
pc_DATA = libpcsclite.pc

SUBDIRS = . utils

lib_LTLIBRARIES = libpcsclite.la libmusclecard.la
noinst_LTLIBRARIES = libpcsclite-core.la
sbin_PROGRAMS = pcscd
check_PROGRAMS = testpcsc muscletest

include_HEADERS = \
	pcsclite.h winscard.h wintypes.h debuglog.h \
	mscdefines.h musclecard.h

if PCSC_ARCH_OSX
PCSC_INCLUDE_LIBS   = -Wl,-framework,IOKit
PCSCD_INCLUDE_LIBS  = -Wl,-framework,IOKit -Wl,-framework,CoreFoundation
endif

PCSC_DYNLOAD_SOURCE = dyn_unix.c dyn_hpux.c dyn_macosx.c dyn_generic.h
PCSC_SYSTEM_SOURCE  = sys_unix.c sys_generic.h
PCSC_THREAD_SOURCE  = thread_unix.c thread_generic.h

if MSC_ENABLE_SCF
PCSC_CLIENT_SRC     = winscard_scf.c
SCF_LIBS = -lsmartcard
SCF_CFLAGS = -I/usr/include/smartcard
else
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

if PCSC_USE_LIBUSB
PCSC_HOTPLUG_SOURCE = hotplug_libusb.c hotplug.h
elif PCSC_LINUX_USB
PCSC_HOTPLUG_SOURCE = hotplug_linux.c hotplug.h
elif PCSC_ARCH_OSX
PCSC_HOTPLUG_SOURCE = hotplug_macosx.c hotplug.h powermgt_macosx.c powermgt_generic.h
else
PCSC_HOTPLUG_SOURCE = hotplug_generic.c hotplug.h
endif

if PCSC_THR_SAFE
PCSC_CLIENT = $(PCSC_THREAD_SOURCE)
else
PCSC_CLIENT = 
endif

libpcsclite_core_la_SOURCES = atrhandler.c atrhandler.h \
	configfile.l configfile.h \
	debuglog.c debuglog.h \
	eventhandler.c eventhandler.h \
	ifdhandler.h \
	ifdwrapper.c ifdwrapper.h \
	prothandler.c prothandler.h \
	readerfactory.c readerfactory.h \
	tokenparser.l parser.h \
	winscard.c winscard.h \
	winscard_msg.c winscard_msg.h \
	$(PCSC_SYSTEM_SOURCE) $(PCSC_THREAD_SOURCE) $(PCSC_HOTPLUG_SOURCE) \
	$(PCSC_DYNLOAD_SOURCE)
libpcsclite_core_la_LDADD = @LIBDL@
libpcsclite_core_la_LDFLAGS = -version-info 0:2:0 $(PCSC_INCLUDE_LIBS)

libpcsclite_la_SOURCES = $(PCSC_CLIENT_SRC) debuglog.c $(PCSC_SYSTEM_SOURCE) \
	winscard_msg.c $(PCSC_CLIENT)
libpcsclite_la_LDFLAGS = -version-info 0:1:0 $(PCSC_INCLUDE_LIBS)

libmusclecard_la_SOURCES = musclecard.c tokenparser.l tokenfactory.c \
	tokenfactory.h $(PCSC_DYNLOAD_SOURCE)
libmusclecard_la_LDFLAGS = -lpcsclite 

pcscd_SOURCES = pcscdaemon.c winscard_svc.c winscard_svc.h
pcscd_LDADD   = libpcsclite-core.la @LIBUSB@ @LIBDL@
pcscd_LDFLAGS = -rdynamic $(PCSCD_INCLUDE_LIBS)

AM_CFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\"

testpcsc_SOURCES = testpcsc.c
testpcsc_LDADD = libpcsclite.la

muscletest_SOURCES = muscletest.c
muscletest_LDADD = libmusclecard.la

# Hack to be able to use flex -P to enable linking of multiple lexer
# sources into one library, until we find a way to make automake handle
# this automagically.  This breaks if lex is not flex!!
tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.tp.c $@ -- "$(LEX)" -Ptp $(AM_LFLAGS) $(LFLAGS)

EXTRA_DIST = README_INTERNALS.txt dyn_win32.c thread_win32.c
