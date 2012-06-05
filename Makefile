include config.mk

all: options $(TARGET)

options: 
	@echo Build options:
	@echo CC 				= $(CC)
	@echo CFLAGS 		= $(CFLAGS)
	@echo LDFLAGS 	= $(LDFLAGS)
	@echo CPPFLAGS 	= $(CPPFLAGS)

$(TARGET): $(SUBDIRS:%=%.subdir-make)

%.subdir-make:
	@$(MAKE) $(MFLAGS) -C $*

#@$(MAKE) -C $(SRCDIR)
#@$(MAKE) -C $(UTILDIR)

clean:  $(SUBDIRS:%=%.subdir-clean)

%.subdir-clean:
	@$(MAKE) $(MFLAGS) clean -C $*

install: $(TARGET) install-man install-data
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(SRCDIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

install-man: all
	install -d $(DESTDIR)$(MAN1DIR)
	install -m 644 $(DOCDIR)/$(MANFILE) $(DESTDIR)$(MAN1DIR)/$(MANFILE)

install-data: all
	@# Lib
	install -d $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBDIR)
	for file in $(LIBDIR)/*; do \
		install -m 644 $$file $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$$file; \
	done
	@# Share
	install -d $(DESTDIR)$(DATADIR)/pixmaps
	install -m 644 $(SHAREDIR)/dwb.png $(DESTDIR)$(DATADIR)/pixmaps/dwb.png
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 $(SHAREDIR)/dwb.desktop $(DESTDIR)$(DATADIR)/applications/dwb.desktop
	@# Libjs
	install -d $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(LIBJSDIR)
	for file in $(LIBJSDIR)/*; do \
		install -m 644 $$file $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$$file; \
	done
	@# Hints
	install -d $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(JSDIR)
	install -m 644 $(JSDIR)/$(HINT_SCRIPT) $(DESTDIR)$(DATADIR)/$(REAL_NAME)/$(JSDIR)/$(HINT_SCRIPT)

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
