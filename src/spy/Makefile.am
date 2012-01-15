AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC

lib_LTLIBRARIES = libpcscspy.la
dist_bin_SCRIPTS = pcsc-spy

libpcscspy_la_SOURCES = \
	pcsc-spy.c
libpcscspy_la_LIBADD = -ldl

man_MANS = pcsc-spy.1

EXTRA_DIST = pcsc-spy.py install_spy.sh uninstall_spy.sh
