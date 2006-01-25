# Process this file with automake to create Makefile.in.

lib_LTLIBRARIES = libmusclecard.la
libmusclecard_la_LDFLAGS = -version-info 1:0:0

noinst_PROGRAMS = muscletest

nobase_include_HEADERS = \
	PCSC/mscdefines.h \
	PCSC/musclecard.h
noinst_HEADERS = \
	tokenfactory.h

libmusclecard_la_SOURCES = \
	musclecard.c \
	tokenfactory.c \
	$(top_srcdir)/src/tokenparser.l \
	$(top_srcdir)/src/dyn_hpux.c \
	$(top_srcdir)/src/dyn_macosx.c \
	$(top_srcdir)/src/dyn_unix.c \
	$(top_srcdir)/src/strlcpy.c \
	$(top_srcdir)/src/strlcat.c \
	$(top_srcdir)/src/thread_unix.c
libmusclecard_la_CFLAGS = \
	-I$(top_srcdir)/src/PCSC -I$(top_builddir)/src/PCSC \
	-I$(top_srcdir)/libmusclecard/src/PCSC \
	$(SYMBOL_VISIBILITY)
libmusclecard_la_LIBADD = \
	$(top_builddir)/src/libpcsclite.la

muscletest_SOURCES = \
	muscletest.c \
	$(top_srcdir)/src/strlcpy.c
muscletest_CFLAGS = \
	-I$(top_srcdir)/src/PCSC -I$(top_builddir)/src/PCSC \
	-I$(top_srcdir)/libmusclecard/src/PCSC
muscletest_LDADD = \
	$(top_builddir)/libmusclecard/src/libmusclecard.la

pcdir= $(libdir)/pkgconfig
pc_DATA = libmusclecard.pc

