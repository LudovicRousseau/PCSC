# Process this file with automake to create Makefile.in.

sbin_PROGRAMS = bundleTool
man_MANS = bundleTool.8

bundleTool_SOURCES = \
	bundleTool.c
bundleTool_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/musclecard \
	$(PCSCLITE_CFLAGS)
bundleTool_LDADD = \
	$(top_builddir)/src/libmusclecard.la \
	$(PCSCLITE_LIBS)

man_in = bundleTool.8.in
EXTRA_DIST = $(man_in)

