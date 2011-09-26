include ../config.mk
OUTFILES=../$(LIBDIR)/keys.html ../$(LIBDIR)/settings.html 
SETTINGS=../$(LIBDIR)/settings.html
KEYS=../$(LIBDIR)/keys.html

all: $(OUTFILES)

$(KEYS): keys.in
	@echo gen keys.html
	@$(shell ./generate_keys.sh $< $@)
	
$(SETTINGS): settings.in
	@echo gen settings.html
	@$(shell ./generate_settings.sh $< $@)

clean: 
	$(RM) $(OUTFILES)

.PHONY: clean
