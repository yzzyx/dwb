include config.mk

all: $(TARGET) 

$(TARGET):
	@$(MAKE) -C $(SRCDIR)

clean: 
	@$(MAKE) clean -C $(SRCDIR)

install: all install-man install-data 
	@echo "Installing $(TARGET) to $(DESTDIR)$(BINDIR)/$(TARGET)"
	@install -Dm 755 $(SRCDIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

install-man: 
	@echo "Installing manpage to $(DESTDIR)$(MAN1DIR)"
	@install -Dm 644 $(DOCDIR)/$(MANFILE) $(DESTDIR)$(MAN1DIR)/$(MANFILE)

install-data: 
	@echo "Installing $(SHAREDIR)/hints.js to $(DESTDIR)$(DATADIR)/$(REAL_NAME)/scripts/hints.js"
	@install -Dm 644 $(SHAREDIR)/hints.js $(DESTDIR)$(DATADIR)/$(REAL_NAME)/scripts/hints.js
	@echo "Installing $(SHAREDIR)/selection.js to $(DESTDIR)$(DATADIR)/$(REAL_NAME)/scripts/selection.js"
	@install -Dm 644 $(SHAREDIR)/selection.js $(DESTDIR)$(DATADIR)/$(REAL_NAME)/scripts/selection.js
	install -Dm 644 $(LIBDIR)/info.html $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/info.html

uninstall: uninstall-man uninstall-data
	@echo "Removing executable from $(subst //,/,$(DESTDIR)$(BINDIR))"
	@$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall-man: 
	@echo "Removing manpage from $(subst //,/,$(DESTDIR)$(MAN1DIR))"
	@$(RM) $(DESTDIR)$(MAN1DIR)$(MANFILE)

uninstall-data: 
	@echo "Removing $(subst //,/,$(DESTDIR)$(DATADIR)/$(REAL_NAME))"
	@$(RM) -r $(DESTDIR)$(DATADIR)/$(REAL_NAME)

distclean: clean

snapshot: 
	@$(MAKE) dist DISTDIR=$(REAL_NAME)-$(BUILDDATE) 

dist: distclean
	@echo "Creating tarball."
	@hg archive -t tgz $(DISTDIR).tar.gz

.PHONY: clean all install uninstall dist  distclean
