# Process this file with automake to create Makefile.in.

SUBDIRS = . example

datadir = $(prefix)/doc
data_DATA = pcsc-lite.pdf muscle-api-1.3.0.pdf README.DAEMON

man_MANS = pcscd.8 bundleTool.8 formaticc.1 reader.conf.5
man_in = pcscd.8.in bundleTool.8.in formaticc.1 reader.conf.5.in

EXTRA_DIST = $(data_DATA) $(man_in)

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
	$(PDFLATEX) $*
	-@egrep -c 'Citation .* undefined.' $*.log && ($(BIBTEX) $* ; $(PDFLATEX) $*)
	-@grep 'Rerun' $*.log && $(PDFLATEX) $*
	-@grep 'Rerun' $*.log && $(PDFLATEX) $*

# create a HTML document
%.html: %.tex
	rm -r $*
	$(LATEX2HTML) -split +2 $*

