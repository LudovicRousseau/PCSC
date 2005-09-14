# Process this file with automake to create Makefile.in.

SUBDIRS = . example

datadir = $(prefix)/doc
data_DATA = pcsc-lite.tex pcsc-lite.bib pcsc-lite.pdf \
			ifdhandler-3.tex ifdhandler-3.pdf \
			README.DAEMON muscle-api-1.3.0.pdf

man_MANS = pcscd.8 formaticc.1 reader.conf.5
man_in = pcscd.8.in formaticc.1 reader.conf.5.in

EXTRA_DIST = $(data_DATA) $(man_in)

CLEANFILES = ifdhandler-3.aux ifdhandler-3.bbl ifdhandler-3.blg \
	ifdhandler-3.dvi ifdhandler-3.log ifdhandler-3.out \
	ifdhandler-3.toc \
	pcsc-lite.aux pcsc-lite.bbl pcsc-lite.blg pcsc-lite.log \
	pcsc-lite.out pcsc-lite.toc

DISTCLEANFILES = ifdhandler-3.pdf pcsc-lite.pdf

# name of commands
LATEX = latex
PDFLATEX = pdflatex
BIBTEX = bibtex
LATEX2HTML = latex2html

# create a dvi file from a tex file
%.dvi: %.tex
	$(LATEX) $*
	-@egrep -c 'Citation .* undefined.' $*.log && ($(BIBTEX) $* ; $(LATEX) $*)
	-@grep 'Rerun' $*.log && $(LATEX) $*
	-@grep 'Rerun' $*.log && $(LATEX) $*

# create a dvi file from a tex file
%.pdf: %.tex
	-$(PDFLATEX) $*
	-@egrep -c 'Citation .* undefined.' $*.log && ($(BIBTEX) $* ; $(PDFLATEX) $*)
	-@grep 'Rerun' $*.log && $(PDFLATEX) $*
	-@grep 'Rerun' $*.log && $(PDFLATEX) $*

# create a HTML document
%.html: %.tex
	rm -rf $*
	$(LATEX2HTML) -local_icons -split +2 $*

