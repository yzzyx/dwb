TARGET=dwbem

CFLAGS += -g -std=c99 -Wall
LDFLAGS += -lreadline

all: $(TARGET)

dwb_em: dwb_em.in
	@echo gen $@
	$(shell sed 's#@SYSTEM_DIR@#$(DATADIR)/$(REAL_NAME)/$(EXTENSIONDIR)#' $< > $@)
	@chmod 755 $@

$(TARGET): dwbem.c
	@echo $(CC) $< -o $@
	@$(CC) $(CFLAGS) $(LDFLAGS) $(shell pkg-config --cflags --libs libsoup-2.4) $< -o $@

clean: 
	rm -f $(TARGET)

.PHONY: clean
