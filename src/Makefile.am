## Process this file with automake to create Makefile.in.
# $Id$

pcdir= @libdir@/pkgconfig
pc_DATA = libpcsclite.pc

SUBDIRS = . utils
lib_LTLIBRARIES = libpcsclite.la
noinst_LTLIBRARIES = libpcsclite-core.la
sbin_PROGRAMS = pcscd
include_HEADERS = mscdefines.h musclecard.h pcsclite.h winscard.h \
	wintypes.h debuglog.h
check_PROGRAMS = testpcsc
TESTS = testpcsc

if PCSC_ARCH_LINUX
if PCSC_LINUX_USB
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -lfl -ldl
PCSCD_INCLUDE_LIBS  = -lfl -ldl -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c
PCSC_DYNLOAD_SOURCE = dyn_unix.c
PCSC_HOTPLUG_SOURCE = hotplug_linux.c
PCSC_SYSTEM_SOURCE  = sys_unix.c
PCSC_CLIENT_SRC     = winscard_clnt.c
else
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -lfl -ldl
PCSCD_INCLUDE_LIBS  = -lfl -ldl -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c
PCSC_DYNLOAD_SOURCE = dyn_unix.c
PCSC_HOTPLUG_SOURCE = hotplug_generic.c
PCSC_SYSTEM_SOURCE  = sys_unix.c
PCSC_CLIENT_SRC     = winscard_clnt.c
endif
endif

if PCSC_ARCH_BSD
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -lfl
PCSCD_INCLUDE_LIBS  = -lfl -pthread
PCSC_THREAD_SOURCE  = thread_unix.c
PCSC_DYNLOAD_SOURCE = dyn_bsd.c
PCSC_HOTPLUG_SOURCE = hotplug_generic.c
PCSC_SYSTEM_SOURCE  = sys_unix.c
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

if PCSC_ARCH_OSX
PCSC_MYLDFLAGS      = -framework CoreFoundation
PCSC_INCLUDE_LIBS   = -ll -lIOKit
PCSCD_INCLUDE_LIBS  = -ll -lIOKit
PCSC_THREAD_SOURCE  = thread_macosx.c
PCSC_DYNLOAD_SOURCE = dyn_macosx.c
PCSC_HOTPLUG_SOURCE = hotplug_macosx.c powermgt_macosx.c
PCSC_SYSTEM_SOURCE  = sys_unix.c
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
PCSC_THREAD_SOURCE  = thread_unix.c
PCSC_DYNLOAD_SOURCE = dyn_unix.c
PCSC_HOTPLUG_SOURCE = hotplug_generic.c
PCSC_SYSTEM_SOURCE  = sys_solaris.c
endif

if PCSC_ARCH_HPUX
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -ll
PCSCD_INCLUDE_LIBS  = -ll -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c
PCSC_DYNLOAD_SOURCE = dyn_hpux.c
PCSC_HOTPLUG_SOURCE = hotplug_generic.c
PCSC_SYSTEM_SOURCE  = sys_hpux.c
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

if PCSC_ARCH_TRU64
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -ll
PCSCD_INCLUDE_LIBS  = -ll -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c
PCSC_DYNLOAD_SOURCE = dyn_unix.c
PCSC_HOTPLUG_SOURCE = hotplug_generic.c
PCSC_SYSTEM_SOURCE  = sys_unix.c
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

if PCSC_ARCH_AIX
PCSC_MYLDFLAGS      =
PCSC_INCLUDE_LIBS   = -ll -lbsd
PCSCD_INCLUDE_LIBS  = -ll -lpthread
PCSC_THREAD_SOURCE  = thread_unix.c
PCSC_DYNLOAD_SOURCE = dyn_unix.c
PCSC_HOTPLUG_SOURCE = hotplug_generic.c
PCSC_SYSTEM_SOURCE  = sys_unix.c
PCSC_CLIENT_SRC     = winscard_clnt.c
endif

if MCARD_ENABLED
MCARD_CLIENT = musclecard.c tokenparser.l tokenfactory.c $(PCSC_DYNLOAD_SOURCE)
else
MCARD_CLIENT =
endif

if PCSC_THR_SAFE
PCSC_CLIENT = $(PCSC_THREAD_SOURCE)
else
PCSC_CLIENT = 
endif

libpcsclite_core_la_SOURCES = configfile.l tokenparser.l \
	winscard.c eventhandler.c ifdwrapper.c atrhandler.c prothandler.c \
	readerfactory.c winscard_msg.c debuglog.c $(PCSC_SYSTEM_SOURCE) \
	$(PCSC_THREAD_SOURCE) $(PCSC_HOTPLUG_SOURCE) $(PCSC_DYNLOAD_SOURCE)

libpcsclite_core_la_LDFLAGS = -version-info 0:2:0 $(PCSC_INCLUDE_LIBS)

libpcsclite_la_SOURCES = $(PCSC_CLIENT_SRC) debuglog.c $(PCSC_SYSTEM_SOURCE) \
			 winscard_msg.c $(PCSC_CLIENT) $(MCARD_CLIENT)

libpcsclite_la_LDFLAGS = $(PCSC_INCLUDE_LIBS) -version-info 0:1:0

pcscd_SOURCES = pcscdaemon.c winscard_svc.c \
	thread_generic.h readerfactory.h \
	atrhandler.h configfile.h dyn_generic.h eventhandler.h hotplug.h \
	ifdhandler.h ifdwrapper.h powermgt_generic.h prothandler.h \
	readerfactory.h sys_generic.h tokenfactory.h winscard_msg.h \
	winscard_svc.h
pcscd_LDADD   = libpcsclite-core.la
pcscd_LDFLAGS = $(PCSC_MYLDFLAGS) -rdynamic $(PCSCD_INCLUDE_LIBS)

AM_CFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\"

testpcsc_SOURCES = testpcsc.c
testpcsc_LDADD = libpcsclite.la

EXTRA_libpcsclite_core_la_SOURCES = dyn_unix.c dyn_macosx.c \
	hotplug_generic.c hotplug_linux.c hotplug_macosx.c \
	thread_unix.c thread_macosx.c dyn_bsd.c dyn_hpux.c \
	sys_solaris.c sys_hpux.c powermgt_macosx.c muscletest.c \
	winscard_scf.c

# Hack to be able to use flex -P to enable linking of multiple lexer
# sources into one library, until we find a way to make automake handle
# this automagically.  This breaks if lex is not flex!!
tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.tp.c $@ -- "$(LEX)" -Ptp $(AM_LFLAGS) $(LFLAGS)

EXTRA_DIST = README_INTERNALS.txt dyn_win32.c

