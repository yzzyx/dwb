include ../config.mk
SETTINGS=../$(LIBDIR)/settings.html
KEYS=../$(LIBDIR)/keys.html
OUTFILES=$(SETTINGS) $(KEYS)

all: $(OUTFILES)

$(KEYS): keys.in
	@echo gen keys.html
	@awk -f generate_keys.awk $< > $@

$(SETTINGS): settings.in
	@echo gen settings.html
	@awk -f generate_settings.awk $< > $@

settings.in: settings.pre
	@$(shell if pkg-config --exists 'libsoup-2.4 >= 2.38'; then \
		sed 's/^SSL_CERTIFICATION/ssl-use-system-ca-file checkbox Whether to use the system certification file/' $< > $@;\
		else  sed 's/^SSL_CERTIFICATION/ssl-ca-file text Path to ssl-certificate/;/^addressbar-dns-lookup/d' $< > $@; fi)

clean: 
	$(RM) $(OUTFILES)

.PHONY: clean
