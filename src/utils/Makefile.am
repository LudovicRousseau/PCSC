# Process this file with automake to create Makefile.in.

AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC

bin_PROGRAMS = formaticc
sbin_PROGRAMS = installifd

installifd_SOURCES = installifd.c

formaticc_SOURCES = formaticc.c
formaticc_LDADD =  $(top_builddir)/src/libpcsclite.la

EXTRA_DIST = sample.in sample.ibm sample.out

