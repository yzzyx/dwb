# See COPYING for copyright and license details

all: man web

man: dwb.1 dwbem.1

web: dwb.1.html dwbem.1.html

#dwb.1: dwb.1.txt 
%.1: %.1.txt
	a2x --doctype manpage --format manpage $<

%.1.html: %.1.txt
	asciidoc -b html5 $<

