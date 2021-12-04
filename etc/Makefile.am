if HAVE_SYSTEMD
EXTRA_DIST = \
	pcscd.service.in \
	pcscd.socket.in

systemdsystemunit_DATA = \
	pcscd.service \
	pcscd.socket

CLEANFILES = $(systemdsystemunit_DATA)

edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@sbindir_exp[@]|$(sbindir_exp)|g' \
	-e 's|@ipcdir[@]|$(ipcdir)|g'

$(systemdsystemunit_DATA): Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod +x $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@

pcscd.service: $(srcdir)/pcscd.service.in
pcscd.socket: $(srcdir)/pcscd.socket.in

endif
