web: jsapi.html

man: dwbscript.7

jsapi.html: jsapi.txt
	@echo gen $@ 
	@asciidoc -b html5 -a toc2 $<

dwb-js.7: jsapi.7.txt
	@echo gen $@ 
	@a2x --doctype manpage --format manpage $<
