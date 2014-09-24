SUBDIRS = src etc doc

fix-rights:
	$(MAKE) -C src $@

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

EXTRA_DIST = \
	GPL-3.0.txt \
	bootstrap \
	ChangeLog.svn \
	DRIVERS SECURITY \
	HELP \
	m4/ax_pthread.m4 \
	m4/as-ac-expand.m4

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

