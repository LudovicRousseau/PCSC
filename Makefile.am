## Process this file with automake to create Makefile.in.
# $Id$

AUTOMAKE_OPTIONS = no-dependencies
SUBDIRS = src etc doc

AUX_DIST = $(ac_aux_dir)/acinclude.m4 \
	$(ac_aux_dir)/aclocal.m4 \
	$(ac_aux_dir)/acx_pthread.m4 \
	$(ac_aux_dir)/config.guess \
	$(ac_aux_dir)/config.sub \
	$(ac_aux_dir)/install-sh \
	$(ac_aux_dir)/ltconfig \
	$(ac_aux_dir)/ltmain.sh \
	$(ac_aux_dir)/missing \
	$(ac_aux_dir)/mkinstalldirs \
	$(ac_aux_dir)/ylwrap

EXTRA_DIST = DRIVERS SECURITY reconf bootstrap $(ac_aux_dir) HELP \
	ChangeLog.cvs
MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config-h.in \
	stamp-h.in $(AUX_DIST)

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ChangeLog.cvs:
	rcs2log | perl -e 'while (<>) { s/corcoran /David Corcoran /; \
	s/oznet /Chris Osgood /; s/cprados /Carlos Prados /; \
	s/mikeg /Michael Gile /; s/rousseau /Ludovic Rousseau /; \
	s+/home/cvsroot/muscle/PCSC/++g; print ; }' > $@

.PHONY: ChangeLog.cvs

