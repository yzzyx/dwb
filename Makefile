include config.mk

all: $(TARGET) 

$(TARGET):
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done

#@$(MAKE) -C $(SRCDIR)
#@$(MAKE) -C $(UTILDIR)

clean: 
	@$(MAKE) clean -C $(SRCDIR)
	@$(MAKE) clean -C $(UTILDIR)

install: all install-man install-data 
	install -Dm 755 $(SRCDIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

install-man: 
	install -Dm 644 $(DOCDIR)/$(MANFILE) $(DESTDIR)$(MAN1DIR)/$(MANFILE)

install-data: 
	install -Dm 644 $(JSDIR)/hints.js $(DESTDIR)$(DATADIR)/$(REAL_NAME)/scripts/hints.js
	install -Dm 644 $(LIBDIR)/$(INFO_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(INFO_FILE)
	install -Dm 644 $(LIBDIR)/$(HEAD_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(HEAD_FILE)
	install -Dm 644 $(LIBDIR)/$(SETTINGS_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(SETTINGS_FILE)
	install -Dm 644 $(LIBDIR)/$(KEY_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(KEY_FILE)
	install -Dm 644 $(LIBDIR)/$(PLUGIN_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(PLUGIN_FILE)
	install -Dm 644 $(LIBDIR)/$(ERROR_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(ERROR_FILE)
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
