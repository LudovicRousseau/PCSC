## Process this file with automake to create Makefile.in.
# $Id$

pcdir= $(libdir)/pkgconfig
pc_DATA = libpcsclite.pc

SUBDIRS = . utils
lib_LTLIBRARIES = libpcsclite.la libmusclecard.la
noinst_LTLIBRARIES = libpcsclite-core.la
sbin_PROGRAMS = pcscd
include_HEADERS = mscdefines.h musclecard.h pcsclite.h winscard.h \
	wintypes.h debuglog.h
check_PROGRAMS = testpcsc muscletest
#TESTS = testpcsc

if PCSC_ARCH_LINUX
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -lfl -ldl
PCSCD_INCLUDE_LIBS  = -lfl -ldl -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c thread_generic.h
PCSC_DYNLOAD_SOURCE = dyn_unix.c dyn_generic.h
PCSC_SYSTEM_SOURCE  = sys_unix.c sys_generic.h
PCSC_CLIENT_SRC     = winscard_clnt.c
if PCSC_USE_LIBUSB
PCSC_HOTPLUG_SOURCE = hotplug_libusb.c hotplug.h
PCSCD_EXTRA_LIB      = -lusb
else
if PCSC_LINUX_USB
PCSC_HOTPLUG_SOURCE = hotplug_linux.c hotplug.h
else
PCSC_HOTPLUG_SOURCE = hotplug_generic.c hotplug.h
endif
endif
endif

if PCSC_ARCH_BSD
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -lfl
PCSCD_INCLUDE_LIBS  = -lfl -pthread
PCSC_THREAD_SOURCE  = thread_unix.c thread_generic.h
PCSC_DYNLOAD_SOURCE = dyn_bsd.c dyn_generic.h
PCSC_SYSTEM_SOURCE  = sys_unix.c sys_generic.h
PCSC_CLIENT_SRC     = winscard_clnt.c
if PCSC_USE_LIBUSB
PCSC_HOTPLUG_SOURCE = hotplug_libusb.c hotplug.h
else
PCSC_HOTPLUG_SOURCE = hotplug_generic.c hotplug.h
endif
endif

if PCSC_ARCH_OSX
PCSC_MYLDFLAGS      = -framework CoreFoundation
PCSC_INCLUDE_LIBS   = -ll -lIOKit
PCSCD_INCLUDE_LIBS  = -ll -lIOKit
PCSC_THREAD_SOURCE  = thread_macosx.c thread_generic.h
PCSC_DYNLOAD_SOURCE = dyn_macosx.c dyn_generic.h
PCSC_HOTPLUG_SOURCE = hotplug_macosx.c hotplug.h powermgt_macosx.c \
					  powermgt_generic.h
PCSC_SYSTEM_SOURCE  = sys_unix.c sys_generic.h
PCSC_CLIENT_SRC     = winscard_clnt.c
INCLUDES  = -I/System/Library/Frameworks/IOKit.framework/Headers/usb \
            -I/System/Library/Frameworks/IOKit.framework/Headers
endif

if PCSC_ARCH_SOLARIS
PCSC_MYLDFLAGS      =

if MSC_ENABLE_SCF
PCSC_INCLUDE_LIBS   = -ll -ldl -lnsl -lsocket -lposix4 -lsmartcard
INCLUDES = -I/usr/include/smartcard
PCSC_CLIENT_SRC     = winscard_scf.c
else
PCSC_INCLUDE_LIBS   = -ll -ldl -lnsl -lsocket -lposix4
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

PCSCD_INCLUDE_LIBS  = -ll -ldl -lnsl -lsocket -lposix4 -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c thread_generic.h
PCSC_DYNLOAD_SOURCE = dyn_unix.c dyn_generic.h
PCSC_HOTPLUG_SOURCE = hotplug_generic.c hotplug.h
PCSC_SYSTEM_SOURCE  = sys_solaris.c sys_generic.h
endif

if PCSC_ARCH_HPUX
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -ll
PCSCD_INCLUDE_LIBS  = -ll -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c thread_generic.h
PCSC_DYNLOAD_SOURCE = dyn_hpux.c dyn_generic.h
PCSC_HOTPLUG_SOURCE = hotplug_generic.c hotplug.h
PCSC_SYSTEM_SOURCE  = sys_hpux.c sys_generic.h
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

if PCSC_ARCH_TRU64
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -ll
PCSCD_INCLUDE_LIBS  = -ll -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c thread_generic.h
PCSC_DYNLOAD_SOURCE = dyn_unix.c dyn_generic.h
PCSC_HOTPLUG_SOURCE = hotplug_generic.c hotplug.h
PCSC_SYSTEM_SOURCE  = sys_unix.c sys_generic.h
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

if PCSC_ARCH_AIX
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -ll -lbsd
PCSCD_INCLUDE_LIBS  = -ll -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c thread_generic.h
PCSC_DYNLOAD_SOURCE = dyn_unix.c dyn_generic.h
PCSC_HOTPLUG_SOURCE = hotplug_generic.c hotplug.h
PCSC_SYSTEM_SOURCE  = sys_unix.c sys_generic.h
PCSC_CLIENT_SRC     = winscard_clnt.c
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
libpcsclite_core_la_LDFLAGS = -version-info 0:2:0 $(PCSC_INCLUDE_LIBS)

libpcsclite_la_SOURCES = $(PCSC_CLIENT_SRC) debuglog.c $(PCSC_SYSTEM_SOURCE) \
	winscard_msg.c $(PCSC_CLIENT)
libpcsclite_la_LDFLAGS = -version-info 0:1:0 $(PCSC_INCLUDE_LIBS)

libmusclecard_la_SOURCES = musclecard.c tokenparser.l tokenfactory.c \
	tokenfactory.h $(PCSC_DYNLOAD_SOURCE)
libmusclecard_la_LDFLAGS = -lpcsclite 

pcscd_SOURCES = pcscdaemon.c winscard_svc.c winscard_svc.h
pcscd_LDADD   = libpcsclite-core.la
pcscd_LDFLAGS = $(PCSC_MYLDFLAGS) -rdynamic $(PCSCD_INCLUDE_LIBS) \
	$(PCSCD_EXTRA_LIB)

AM_CFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\"

testpcsc_SOURCES = testpcsc.c
testpcsc_LDADD = libpcsclite.la

muscletest_SOURCES = muscletest.c
muscletest_LDADD = libmusclecard.la

# EXTRA_DIST = dyn_unix.c dyn_macosx.c \
# 	hotplug_generic.c hotplug_linux.c hotplug_libusb.c hotplug_macosx.c \
# 	thread_unix.c thread_macosx.c dyn_bsd.c dyn_hpux.c \
# 	sys_solaris.c sys_hpux.c powermgt_macosx.c \
# 	winscard_scf.c

# Hack to be able to use flex -P to enable linking of multiple lexer
# sources into one library, until we find a way to make automake handle
# this automagically.  This breaks if lex is not flex!!
tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.tp.c $@ -- "$(LEX)" -Ptp $(AM_LFLAGS) $(LFLAGS)

EXTRA_DIST = README_INTERNALS.txt dyn_win32.c

