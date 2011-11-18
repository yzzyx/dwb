include ../config.mk
SETTINGS=../$(LIBDIR)/settings.html
KEYS=../$(LIBDIR)/keys.html
TLDS=../$(LIBDIR)/tlds.txt
OUTFILES=$(SETTINGS) $(KEYS) $(TLDS)

all: $(OUTFILES)

$(KEYS): keys.in
	@echo gen keys.html
	@$(shell ./generate_keys.sh $< $@)
	
$(SETTINGS): settings.in
	@echo gen settings.html
	@$(shell ./generate_settings.sh $< $@)

$(TLDS): tlds.in
	@echo gen tlds.txt
	@sed '/^\/\/\|^\s*$$/d' $< > $@

clean: 
	$(RM) $(OUTFILES)

.PHONY: clean
