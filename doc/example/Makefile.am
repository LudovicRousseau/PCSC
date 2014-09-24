# Process this file with automake to create Makefile.in.

noinst_PROGRAMS = pcsc_demo
pcsc_demo_SOURCES = pcsc_demo.c
pcsc_demo_CFLAGS = -I$(top_srcdir)/src/PCSC -I$(top_builddir)/src/PCSC
pcsc_demo_LDADD = $(top_builddir)/src/libpcsclite.la
