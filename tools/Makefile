# See COPYING for copyright and license details

include ../config.mk
TARGET=dwbem

OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
LIBS=libsoup-2.4 glib-2.0

SYSTEM_EXTENSION_DIR=$(PREFIX)/share/dwb/extensions

CFLAGS := $(CFLAGS)
CFLAGS += -std=c99 
CFLAGS += -Wall -g
CFLAGS += -Wextra -Werror=format-security 
CFLAGS += $(shell pkg-config --cflags $(LIBS))
CFLAGS += -DSYSTEM_EXTENSION_DIR=\"$(SYSTEM_EXTENSION_DIR)\"


LDFLAGS := $(LDFLAGS)
LDFLAGS += $(shell pkg-config --libs $(LIBS))

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo $(CC) $@
	@$(CC) $< -o $@ $(LDFLAGS)

%.o: %.c
	@echo $(CC) $<
	@$(CC) $(CFLAGS) -c $< -o $@ $(CPPFLAGS)

clean: 
	rm -f $(TARGET) $(OBJ)

.PHONY: clean
