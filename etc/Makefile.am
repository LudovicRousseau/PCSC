# Process this file with automake to create Makefile.in.

conf_DATA = reader.conf
sbin_SCRIPTS = update-reader.conf

EXTRA_DIST = StartupParameters.plist SmartcardServices

install-data-hook:
	$(mkdir_p) "$(DESTDIR)$(confdir)/reader.conf.d"

