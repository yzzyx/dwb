include config.mk

all: $(TARGET) 

$(TARGET):
	@$(MAKE) -C $(SRCDIR)

clean: 
	@$(MAKE) clean -C $(SRCDIR)

install: all install-man install-data 
	install -Dm 755 $(SRCDIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

install-man: 
	install -Dm 644 $(DOCDIR)/$(MANFILE) $(DESTDIR)$(MAN1DIR)/$(MANFILE)

install-data: 
	install -Dm 644 $(JSDIR)/hints.js $(DESTDIR)$(DATADIR)/$(REAL_NAME)/scripts/hints.js
	install -Dm 644 $(JSDIR)/selection.js $(DESTDIR)$(DATADIR)/$(REAL_NAME)/scripts/selection.js
	install -Dm 644 $(LIBDIR)/info.html $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/info.html
	install -Dm 644 $(LIBDIR)/head.html $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/head.html
	install -Dm 644 $(LIBDIR)/settings.html $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/settings.html
	install -Dm 644 $(LIBDIR)/keys.html $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/keys.html
	install -Dm 644 $(SHAREDIR)/dwb.png $(DESTDIR)$(DATADIR)/pixmaps/dwb.png
	install -Dm 644 $(SHAREDIR)/dwb.desktop $(DESTDIR)$(DATADIR)/applications/dwb.desktop

uninstall: uninstall-man uninstall-data
	@echo "Removing executable from $(subst //,/,$(DESTDIR)$(BINDIR))"
	@$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall-man: 
	$(RM) $(DESTDIR)$(MAN1DIR)/$(MANFILE)

uninstall-data: 
	$(RM) -r $(DESTDIR)$(DATADIR)/$(REAL_NAME)
	$(RM) -r $(DESTDIR)$(DATADIR)/applications/dwb.desktop
	$(RM) -r $(DESTDIR)$(DATADIR)/pixmaps/dwb.png

distclean: clean

snapshot: 
	@$(MAKE) dist DISTDIR=$(REAL_NAME)-$(BUILDDATE) 

dist: distclean
	@echo "Creating tarball."
	@hg archive -t tgz $(DISTDIR).tar.gz

.PHONY: clean all install uninstall dist  distclean
