# Process this file with automake to create Makefile.in.

lib_LTLIBRARIES = libmusclecard.la
libmusclecard_la_LDFLAGS = -version-info 1:0:0

noinst_PROGRAMS = muscletest

nobase_include_HEADERS = \
	PCSC/mscdefines.h \
	PCSC/musclecard.h
noinst_HEADERS = \
	tokenfactory.h \
	strlcpycat.h

if NEED_STRLCPY
STRLCPY=strlcpy.c
endif
                                                                                                    
if NEED_STRLCAT
STRLCAT=strlcat.c
endif

libmusclecard_la_SOURCES = \
	musclecard.c \
	tokenfactory.c \
	$(STRLCPY) \
	$(STRLCAT)
libmusclecard_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/PCSC \
	$(PCSCLITE_CFLAGS)
libmusclecard_la_LIBADD = \
	$(PCSCLITE_LIBS)

muscletest_SOURCES = \
	muscletest.c
muscletest_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/PCSC \
	$(PCSCLITE_CFLAGS)
muscletest_LDADD = \
	$(top_builddir)/src/libmusclecard.la \
	$(PCSCLITE_LIBS)

pcdir= $(libdir)/pkgconfig
pc_DATA = libmusclecard.pc

