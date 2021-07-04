SUBDIRS = src etc doc

fix-rights:
	$(MAKE) -C src $@

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

EXTRA_DIST = \
	GPL-3.0.txt \
	bootstrap \
	ChangeLog.git \
	SECURITY \
	HELP

DISTCLEANFILES = ChangeLog.git

ChangeLog.git:
	git log --stat --decorate=short > $@

