# Process this file with automake to create Makefile.in.

SUBDIRS = aclocal win32 src etc doc

EXTRA_DIST = DRIVERS SECURITY bootstrap HELP ChangeLog.cvs

DISTCLEANFILES = ChangeLog.cvs

ChangeLog.cvs:
	rcs2log | perl -e 'while (<>) { s/corcoran /David Corcoran /; \
	s/corcoran-guest /David Corcoran /; \
	s/oznet /Chris Osgood /; s/cprados /Carlos Prados /; \
	s/mikeg /Michael Gile /; s/rousseau /Ludovic Rousseau /; \
	s/giraud /Jean-Luc Giraud /; \
	s+/cvsroot/pcsclite/PCSC/++g; print ; }' > $@

.PHONY: ChangeLog.cvs
