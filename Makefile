include config.mk

all: options $(TARGET)

options: 
	@echo Build options:
	@echo CC 				= $(CC)
	@echo CFLAGS 		= $(CFLAGS)
	@echo LDFLAGS 	= $(LDFLAGS)
	@echo CPPFLAGS 	= $(CPPFLAGS)

$(TARGET): 
	@for dir in $(SUBDIRS); do $(MAKE) $(MFLAGS) -C $$dir; done

#@$(MAKE) -C $(SRCDIR)
#@$(MAKE) -C $(UTILDIR)

clean: 
	@echo Cleaning 
	@for dir in $(SUBDIRS); do $(MAKE) clean -C $$dir; done

install: $(TARGET) install-man install-data
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(SRCDIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

install-man: all
	install -d $(DESTDIR)$(MAN1DIR)
	install -m 644 $(DOCDIR)/$(MANFILE) $(DESTDIR)$(MAN1DIR)/$(MANFILE)

install-data: all
	install -d $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(JSDIR)
	install -m 644 $(JSDIR)/$(HINT_SCRIPT) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(JSDIR)/$(HINT_SCRIPT)
	install -d $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBJSDIR)
	install -m 644 $(LIBJSFILES) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBJSDIR)/
	install -d $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)
	install -m 644 $(LIBDIR)/$(INFO_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(INFO_FILE)
	install -m 644 $(LIBDIR)/$(HEAD_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(HEAD_FILE)
	install -m 644 $(LIBDIR)/$(SETTINGS_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(SETTINGS_FILE)
	install -m 644 $(LIBDIR)/$(KEY_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(KEY_FILE)
	install -m 644 $(LIBDIR)/$(PLUGIN_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(PLUGIN_FILE)
	install -m 644 $(LIBDIR)/$(ERROR_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(ERROR_FILE)
	install -m 644 $(LIBDIR)/$(LOCAL_FILE) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)/$(LOCAL_FILE)
	install -d $(DESTDIR)$(DATADIR)/pixmaps
	install -m 644 $(SHAREDIR)/dwb.png $(DESTDIR)$(DATADIR)/pixmaps/dwb.png
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 $(SHAREDIR)/dwb.desktop $(DESTDIR)$(DATADIR)/applications/dwb.desktop

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

.PHONY: clean install uninstall distclean install-data install-man uninstall-man uninstall-data phony options
