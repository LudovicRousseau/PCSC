# Makefile for utilities
# David Corcoran <corcoran@linuxnet.com>

INCLUDES = -I$(top_srcdir)/src

bin_PROGRAMS = installifd formaticc
installifd_SOURCES = installifd.c
formaticc_SOURCES = formaticc.c
formaticc_LDADD =  $(top_builddir)/src/libpcsclite.la

EXTRA_DIST = sample.in sample.ibm sample.out

