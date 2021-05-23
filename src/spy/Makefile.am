AM_CPPFLAGS = -I$(top_srcdir)/src/PCSC -I$(top_builddir)/src/PCSC

lib_LTLIBRARIES = libpcscspy.la
dist_bin_SCRIPTS = pcsc-spy

libpcscspy_la_SOURCES = \
	libpcscspy.c

if ENABLE_DOC
man_MANS = pcsc-spy.1

pcsc-spy.1: pcsc-spy.pod
	pod2man \
		--center="PC/SC lite" \
		--release="$(PACKAGE_NAME) $(PACKAGE_VERSION)" \
		$(srcdir)/pcsc-spy.pod > $@
endif

EXTRA_DIST = install_spy.sh uninstall_spy.sh pcsc-spy.pod
CLEANFILES = pcsc-spy.1
