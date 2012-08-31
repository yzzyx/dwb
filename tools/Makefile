TARGET=dwbem

OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
LIBS=libsoup-2.4 glib-2.0

PREFIX ?= /usr

SYSTEM_EXTENSION_DIR=$(PREFIX)/share/dwb/extensions

CFLAGS := $(CFLAGS)
CFLAGS += -std=c99 
CFLAGS += -Wall
CFLAGS += $(shell pkg-config --cflags $(LIBS))
CFLAGS += -DSYSTEM_EXTENSION_DIR=\"$(SYSTEM_EXTENSION_DIR)\"


LDFLAGS := $(LDFLAGS)
LDFLAGS += -lreadline
LDFLAGS += $(shell pkg-config --libs $(LIBS))

all: $(TARGET)

dwb_em: dwb_em.in
	@echo gen $@
	$(shell sed 's#@SYSTEM_DIR@#$(DATADIR)/$(REAL_NAME)/$(EXTENSIONDIR)#' $< > $@)
	@chmod 755 $@

$(TARGET): $(OBJ)
	@echo $(CC) $@
	@$(CC) $< -o $@ $(LDFLAGS)

%.o: %.c
	@echo $(CC) $<
	@$(CC) $(CFLAGS) -c $< -o $@ $(CPPFLAGS)

clean: 
	rm -f $(TARGET)

.PHONY: clean
