# Process this file with automake to create Makefile.in.

SUBDIRS = . utils
AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC

if HAVE_MUSCLECARD
MC_LIBS = libmusclecard.la
MC_BIN = muscletest
MC_HDR = PCSC/mscdefines.h PCSC/musclecard.h
endif

lib_LTLIBRARIES = libpcsclite.la $(MC_LIBS)
noinst_LTLIBRARIES = libpcsclite-core.la

sbin_PROGRAMS = pcscd
noinst_PROGRAMS = testpcsc $(MC_BIN)

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
	$(PCSC_POWERMGT_SRC)
libpcsclite_core_la_CFLAGS = @CFLAGS@ @PTHREAD_CFLAGS@

libpcsclite_la_SOURCES = \
	debuglog.c \
	winscard_msg.c \
	$(PCSC_CLIENT_SRC) \
	$(PCSC_SYSTEM_SRC) \
	$(PCSC_THREAD_SRC)
libpcsclite_la_LDFLAGS = -version-info 1:0:0
libpcsclite_la_LIBADD = @COREFOUNDATION@ @LIBSMARTCARD@

libmusclecard_la_SOURCES = \
	musclecard.c \
	tokenparser.l \
	tokenfactory.c \
	$(PCSC_DYNLOAD_SRC)
libmusclecard_la_LDFLAGS = -version-info 1:0:0
libmusclecard_la_LIBADD = libpcsclite.la @LIBDL@ @LIBFL@

pcscd_SOURCES = pcscdaemon.c winscard_svc.c
pcscd_CFLAGS = @CFLAGS@ @PTHREAD_CFLAGS@
pcscd_LDFLAGS = @LDFLAGS@ -rdynamic
pcscd_LDADD = libpcsclite-core.la \
	@PTHREAD_LIBS@ @COREFOUNDATION@ \
	@LIBUSB@ @LIBDL@ @LIBFL@ @IOKIT@

testpcsc_SOURCES = testpcsc.c
testpcsc_LDADD = libpcsclite.la

muscletest_SOURCES = muscletest.c
muscletest_LDADD = libmusclecard.la

nobase_include_HEADERS = \
	PCSC/debuglog.h PCSC/ifdhandler.h PCSC/pcsclite.h PCSC/winscard.h \
	PCSC/wintypes.h PCSC/parser.h $(MC_HDR)
noinst_HEADERS = \
	atrhandler.h configfile.h dyn_generic.h eventhandler.h \
	hotplug.h ifdwrapper.h PCSC/pcsclite.h.in \
	powermgt_generic.h prothandler.h readerfactory.h \
	sys_generic.h thread_generic.h tokenfactory.h \
	winscard_msg.h winscard_svc.h

pcdir= $(libdir)/pkgconfig
pc_DATA = libpcsclite.pc libmusclecard.pc

# Hack to be able to use flex -P to enable linking of multiple lexer
# sources into one library, until we find a way to make automake handle
# this automagically.  This breaks if lex is not flex!!
tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.tp.c $@ -- "$(LEX)" -Ptp $(AM_LFLAGS) $(LFLAGS)

EXTRA_DIST = README_INTERNALS.txt dyn_win32.c thread_win32.c winscard_scf.c

