# Process this file with automake to create Makefile.in.

AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC

noinst_PROGRAMS = pcsc_demo
pcsc_demo_SOURCES = pcsc_demo.c
pcsc_demo_LDADD =  $(top_builddir)/src/libpcsclite.la

