# Process this file with automake to create Makefile.in.

AUTOMAKE_OPTIONS = no-dependencies
SUBDIRS = src etc doc

AUX_DIST = \
	$(ac_aux_dir)/aclocal.m4 \
	$(ac_aux_dir)/config.guess \
	$(ac_aux_dir)/config.sub \
	$(ac_aux_dir)/depcomp \
	$(ac_aux_dir)/install-sh \
	$(ac_aux_dir)/ltmain.sh \
	$(ac_aux_dir)/missing \
	$(ac_aux_dir)/mkinstalldirs \
	$(ac_aux_dir)/ylwrap

WIN32 = \
	win32/PCSC.cpp \
	win32/PCSC.dsp \
	win32/PCSC.h \
	win32/pthread.h \
	win32/sched.h \
	win32/StdAfx.cpp \
	win32/StdAfx.h \
	win32/win32_config.h \
	win32/win32_pcsclite.h

EXTRA_DIST = DRIVERS SECURITY reconf bootstrap HELP ChangeLog.cvs \
	$(AUX_DIST) $(ac_aux_dir)/acx_pthread.m4 $(WIN32)

MAINTAINERCLEANFILES = $(AUX_DIST)
DISTCLEANFILES = ChangeLog.cvs

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ChangeLog.cvs:
	rcs2log | perl -e 'while (<>) { s/corcoran /David Corcoran /; \
	s/corcoran-guest /David Corcoran /; \
	s/oznet /Chris Osgood /; s/cprados /Carlos Prados /; \
	s/mikeg /Michael Gile /; s/rousseau /Ludovic Rousseau /; \
	s/giraud /Jean-Luc Giraud /; \
	s+/cvsroot/pcsclite/PCSC/++g; print ; }' > $@

.PHONY: ChangeLog.cvs

