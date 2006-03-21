# Process this file with automake to create Makefile.in.

SUBDIRS = . utils
AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC -I$(top_builddir)/src/PCSC \
	$(SYMBOL_VISIBILITY)

lib_LTLIBRARIES = libpcsclite.la

if !HAVE_SCF
sbin_PROGRAMS = pcscd
endif
noinst_PROGRAMS = testpcsc

if HAVE_SCF
PCSC_CLIENT_SRC  = winscard_scf.c
else
PCSC_CLIENT_SRC  = winscard_clnt.c
endif

libpcsclite_la_SOURCES = \
	debug.c \
	dyn_hpux.c \
	dyn_macosx.c \
	dyn_unix.c \
	error.c \
	$(PCSC_CLIENT_SRC) \
	strlcat.c \
	strlcpy.c \
	sys_unix.c \
	thread_unix.c \
	winscard_msg.c
libpcsclite_la_LDFLAGS = -version-info 1:0:0
libpcsclite_la_CFLAGS = $(CFLAGS) $(PTHREAD_CFLAGS)
libpcsclite_la_LIBADD = $(COREFOUNDATION) $(LIBSMARTCARD) $(LIBDL) \
	$(PTHREAD_CFLAGS) $(PTHREAD_LIBS)

pcscd_SOURCES = \
	atrhandler.c \
	configfile.l \
	debuglog.c \
	eventhandler.c \
	ifdwrapper.c \
	pcscdaemon.c \
	dyn_unix.c \
	dyn_hpux.c \
	dyn_macosx.c \
	hotplug_generic.c \
	hotplug_libusb.c \
	hotplug_linux.c \
	hotplug_macosx.c \
	powermgt_generic.c \
	powermgt_macosx.c \
	sys_unix.c \
	thread_unix.c \
	prothandler.c \
	readerfactory.c \
	strlcat.c \
	strlcpy.c \
	tokenparser.l \
	winscard.c \
	winscard_msg.c \
	winscard_msg_srv.c \
	winscard_svc.c
pcscd_CFLAGS = $(CFLAGS) $(PTHREAD_CFLAGS) -DPCSCD
pcscd_LDFLAGS = $(LDFLAGS) -export-dynamic
pcscd_LDADD = \
	$(PTHREAD_LIBS) $(COREFOUNDATION) \
	$(LIBUSB_LIBS) $(LIBDL) $(IOKIT) \
	$(PTHREAD_LIBS) $(PTHREAD_CFLAGS)

testpcsc_SOURCES = testpcsc.c
testpcsc_LDADD = libpcsclite.la

nobase_include_HEADERS = \
	PCSC/debuglog.h \
	PCSC/ifdhandler.h \
	PCSC/winscard.h \
	PCSC/wintypes.h
noinst_HEADERS = \
	atrhandler.h \
	configfile.h \
	debug.h \
	dyn_generic.h \
	eventhandler.h \
	hotplug.h \
	ifdwrapper.h \
	misc.h \
	parser.h \
	PCSC/pcsclite.h.in \
	powermgt_generic.h \
	prothandler.h \
	readerfactory.h \
	strlcpycat.h \
	sys_generic.h \
	thread_generic.h \
	winscard_msg.h \
	winscard_svc.h

nodistheaderdir = $(includedir)/PCSC
nodistheader_DATA = PCSC/reader.h PCSC/pcsclite.h

pcdir= $(libdir)/pkgconfig
pc_DATA = libpcsclite.pc

# Hack to be able to use flex -P to enable linking of multiple lexer
# sources into one library, until we find a way to make automake handle
# this automagically.  This breaks if lex is not flex!!
tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.tp.c $@ -- "$(LEX)" -Ptp $(AM_LFLAGS) $(LFLAGS)

EXTRA_DIST = README_INTERNALS.txt dyn_win32.c thread_win32.c winscard_scf.c

