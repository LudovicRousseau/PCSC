SUBDIRS = m4 src doc

fix-rights:
	$(MAKE) -C src $@

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = DRIVERS SECURITY bootstrap HELP ChangeLog.svn

DISTCLEANFILES = ChangeLog.svn

ChangeLog.svn:
	-svn2cl --group-by-day --stdout --include-rev --reparagraph | perl -pe \
	's/  corcoran-guest/  David Corcoran/; \
	s/  corcoran/  David Corcoran/; \
	s/  oznet/  Chris Osgood/; \
	s/  cprados/  Carlos Prados/; \
	s/  mikeg/  Michael Gile/; \
	s/  rousseau/  Ludovic Rousseau/; \
	s/  giraud/  Jean-Luc Giraud/; \
	s/  aet-guest/  Antti Tapaninen/; \
	s/  sauveron-guest/  Damien Sauveron/; \
	s+trunk/PCSC/++g;' > $@

