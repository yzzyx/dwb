TARGET = dwb
MANFILE = dwb.1
SRCDIR = src
DOCDIR = doc
PREFIX = /usr
DATADIR = $(PREFIX)/share
MANUALDIR = $(DATADIR)/man
MAKE = make --no-print-directory

all: $(TARGET) 

$(TARGET):
	@$(MAKE) -C $(SRCDIR)

clean: 
	@$(MAKE) clean -C $(SRCDIR)

install: all
	@echo "Installing $(TARGET) to $(DESTDIR)$(PREFIX)/bin"
	@install -Dm 755 $(SRCDIR)/$(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@echo "Installing manpage to $(DESTDIR)$(MANUALDIR)/man1"
	@install -Dm 644 $(DOCDIR)/$(MANFILE) $(DESTDIR)$(MANUALDIR)/man1/$(MANFILE)

uninstall:
	@echo "Removing executable from $(PREFIX)/bin/$(TARGET)"
	@rm -f $(PREFIX)/bin/$(TARGET)
	@echo "Removing manpage from $(DESTDIR)$(MANUALDIR)/man1"
	@rm -f $(DESTDIR)$(MANUALDIR)/man1/$(MANFILE)

.PHONY: clean all
