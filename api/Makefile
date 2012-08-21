all: web man webman

web: jsapi.html

man: dwb-js.7

webman: jsapi.7.html

jsapi.html: jsapi.txt
	asciidoc -b html5 -a toc2 $<

jsapi.7.html: jsapi.7.txt
	asciidoc -b html5 -a toc2 $<

dwb-js.7: jsapi.7.txt
	a2x -v --doctype manpage --format manpage $<
