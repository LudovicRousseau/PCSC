# Process this file with automake to create Makefile.in.

SUBDIRS = . example

doc_DATA = \
	README.DAEMON

man_MANS = pcscd.8 reader.conf.5 
man_in = pcscd.8.in reader.conf.5.in

EXTRA_DIST = $(doc_DATA) $(man_in) doxygen.conf.in formaticc.1

doxygen:
	rm -fr api ; cd .. ; doxygen doc/doxygen.conf
#	cp doxygen.css api/
