all: web man

web: jsapi.html

man: dwb-js.7

jsapi.html: jsapi.txt
	asciidoc -b html5 -a toc2 $<

dwb-js.7: jsapi.7.txt
	a2x --doctype manpage --format manpage $<
