# Process this file with automake to create Makefile.in.

SUBDIRS = m4 win32 src etc doc libmusclecard

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = DRIVERS SECURITY bootstrap HELP ChangeLog.cvs

DISTCLEANFILES = ChangeLog.cvs

ChangeLog.cvs:
	svn2cl --group-by-day | perl -pe 's/corcoran /David Corcoran /; \
	s/corcoran-guest /David Corcoran /; \
	s/oznet /Chris Osgood /; s/cprados /Carlos Prados /; \
	s/mikeg /Michael Gile /; s/rousseau /Ludovic Rousseau /; \
	s/giraud /Jean-Luc Giraud /; s/aet-guest /Antti Tapaninen /; \
	s/sauveron-guest /Damien Sauveron /; \
	s+trunk/++g;' > $@

