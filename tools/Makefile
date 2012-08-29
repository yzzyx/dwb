include ../config.mk

TARGET=dwb_em

all: $(TARGET)

$(TARGET): dwb_em.in
	@echo gen $@
	$(shell sed 's#@SYSTEM_DIR@#$(DATADIR)/$(REAL_NAME)/$(EXTENSIONDIR)#' $< > $@)
	@chmod 755 $@

clean: 
	rm -f $(TARGET)

.PHONY: clean
