AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC

lib_LTLIBRARIES = libpcscspy.la

libpcscspy_la_SOURCES = \
	pcsc-spy.c
libpcscspy_la_LIBADD = -ldl

EXTRA_DIST = pcsc-spy.py install_spy.sh uninstall_spy.sh README.txt
