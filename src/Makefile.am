# Process this file with automake to create Makefile.in.

SUBDIRS = . utils
AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC

lib_LTLIBRARIES = libpcsclite.la
noinst_LTLIBRARIES = libpcsclite-core.la

sbin_PROGRAMS = pcscd
noinst_PROGRAMS = testpcsc

PCSC_DYNLOAD_SRC = dyn_unix.c dyn_hpux.c dyn_macosx.c
PCSC_SYSTEM_SRC  = sys_unix.c
PCSC_THREAD_SRC  = thread_unix.c
PCSC_HOTPLUG_SRC  = \
	hotplug_generic.c hotplug_libusb.c \
	hotplug_linux.c hotplug_macosx.c
PCSC_POWERMGT_SRC = powermgt_generic.c powermgt_macosx.c

if HAVE_SCF
PCSC_CLIENT_SRC  = winscard_scf.c
else
PCSC_CLIENT_SRC  = winscard_clnt.c
endif

if NEED_STRLCPY
STRLCPY=strlcpy.c
endif

if NEED_STRLCAT
STRLCAT=strlcat.c
endif

libpcsclite_core_la_SOURCES = \
	atrhandler.c \
	configfile.l \
	debuglog.c \
	eventhandler.c \
	ifdwrapper.c \
	prothandler.c \
	readerfactory.c \
	tokenparser.l \
	winscard.c \
	winscard_msg.c \
	$(PCSC_SYSTEM_SRC) \
	$(PCSC_THREAD_SRC) \
	$(PCSC_DYNLOAD_SRC) \
	$(PCSC_HOTPLUG_SRC) \
	$(PCSC_POWERMGT_SRC) \
	$(STRLCPY) \
	$(STRLCAT)
libpcsclite_core_la_CFLAGS = @CFLAGS@ @PTHREAD_CFLAGS@

libpcsclite_la_SOURCES = \
	debuglog.c \
	winscard_msg.c \
	tokenparser.l \
	$(PCSC_CLIENT_SRC) \
	$(PCSC_SYSTEM_SRC) \
	$(PCSC_THREAD_SRC) \
	$(PCSC_DYNLOAD_SRC) \
	$(STRLCPY) \
	$(STRLCAT)
libpcsclite_la_LDFLAGS = -version-info 1:0:0
libpcsclite_la_LIBADD = @COREFOUNDATION@ @LIBSMARTCARD@ @LIBDL@

pcscd_SOURCES = pcscdaemon.c winscard_svc.c
pcscd_CFLAGS = @CFLAGS@ @PTHREAD_CFLAGS@
pcscd_LDFLAGS = @LDFLAGS@ -rdynamic
pcscd_LDADD = libpcsclite-core.la \
	@PTHREAD_LIBS@ @COREFOUNDATION@ \
	@LIBUSB@ @LIBDL@ @LIBFL@ @IOKIT@

testpcsc_SOURCES = testpcsc.c
testpcsc_LDADD = libpcsclite.la

nobase_include_HEADERS = \
	PCSC/debuglog.h PCSC/dyn_generic.h PCSC/ifdhandler.h PCSC/pcsclite.h \
	PCSC/winscard.h PCSC/wintypes.h PCSC/parser.h \
	PCSC/sys_generic.h PCSC/thread_generic.h
noinst_HEADERS = \
	atrhandler.h configfile.h eventhandler.h \
	hotplug.h ifdwrapper.h PCSC/pcsclite.h.in \
	powermgt_generic.h prothandler.h readerfactory.h \
	strlcpycat.h \
	winscard_msg.h winscard_svc.h

pcdir= $(libdir)/pkgconfig
pc_DATA = libpcsclite.pc

# Hack to be able to use flex -P to enable linking of multiple lexer
# sources into one library, until we find a way to make automake handle
# this automagically.  This breaks if lex is not flex!!
tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.tp.c $@ -- "$(LEX)" -Ptp $(AM_LFLAGS) $(LFLAGS)

EXTRA_DIST = README_INTERNALS.txt dyn_win32.c thread_win32.c winscard_scf.c

