if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	pcscd.service \
	pcscd.socket

edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@sbindir_exp[@]|$(sbindir_exp)|g' \
	-e 's|@ipcdir[@]|$(ipcdir)|g'

$(systemdsystemunit_DATA): % : %.in
	$(edit) $< >$@
endif
