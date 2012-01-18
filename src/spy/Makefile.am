AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC

lib_LTLIBRARIES = libpcscspy.la
dist_bin_SCRIPTS = pcsc-spy

libpcscspy_la_SOURCES = \
	libpcscspy.c
libpcscspy_la_LIBADD = -ldl

man_MANS = pcsc-spy.1

%.1: %.pod
	pod2man \
		--center="PC/SC lite" \
		--release="$(PACKAGE_NAME) $(PACKAGE_VERSION)" \
		$^ > $@

EXTRA_DIST = install_spy.sh uninstall_spy.sh pcsc-spy.pod
CLEANFILES = pcsc-spy.1
