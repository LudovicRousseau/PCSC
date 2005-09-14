# Process this file with automake to create Makefile.in.

sbin_PROGRAMS = bundleTool
man_MANS = bundleTool.8

bundleTool_SOURCES = \
	bundleTool.c
bundleTool_CFLAGS = \
	-I$(top_srcdir)/src/PCSC -I$(top_builddir)/src/PCSC
bundleTool_LDADD = \
	$(top_builddir)/src/libpcsclite.la

man_in = bundleTool.8.in
EXTRA_DIST = $(man_in)

