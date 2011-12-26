include ../config.mk
SETTINGS=../$(LIBDIR)/settings.html
KEYS=../$(LIBDIR)/keys.html
TLDS=../$(SRCDIR)/tlds.h
OUTFILES=$(SETTINGS) $(KEYS) $(TLDS)

all: $(OUTFILES)

$(KEYS): keys.in
	@echo gen keys.html
	@awk -f generate_keys.awk $< > $@

$(SETTINGS): settings.in
	@echo gen settings.html
	@awk -f generate_settings.awk $< > $@

clean: 
	$(RM) $(OUTFILES)

.PHONY: clean
