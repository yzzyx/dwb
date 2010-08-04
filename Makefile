include config.mk

all: $(TARGET) 

$(TARGET):
	@$(MAKE) -C $(SRCDIR)

clean: 
	@$(MAKE) clean -C $(SRCDIR)

install: install-man install-data 
	@echo "Installing $(TARGET) to $(subst //,/,$(DESTDIR)$(BINDIR))"
	@install -Dm 755 $(SRCDIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

install-man: 
	@echo "Installing manpage to $(subst //,/,$(DESTDIR)$(MAN1DIR))"
	@install -Dm 644 $(DOCDIR)/$(MANFILE) $(DESTDIR)$(MAN1DIR)$(MANFILE)

install-data: 
	@echo "Installing $(SHAREDIR)/hints.js to $(subst //,/,$(DESTDIR)$(DATADIR)/$(NAME)/scripts)"
	@install -Dm 644 $(SHAREDIR)/hints.js $(DESTDIR)$(DATADIR)/$(NAME)/scripts/hints.js

uninstall: uninstall-man uninstall-data
	@echo "Removing executable from $(subst //,/,$(DESTDIR)$(BINDIR))"
	@$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall-man: 
	@echo "Removing manpage from $(subst //,/,$(DESTDIR)$(MAN1DIR))"
	@$(RM) $(DESTDIR)$(MAN1DIR)$(MANFILE)

uninstall-data: 
	@echo "Removing $(subst //,/,$(DESTDIR)$(DATADIR)/$(NAME))"
	@$(RM) -r $(DESTDIR)$(DATADIR)/$(NAME)

distclean: clean
	@echo "Creating tarball."

dist: distclean
	@mkdir -p $(DISTDIR)
	@cp Makefile README gpl-3.0.txt config.mk $(DISTDIR)
	@mkdir -p $(DISTDIR)/$(DOCDIR)
	@cp $(DOCDIR)/dwb.1 $(DISTDIR)/$(DOCDIR)
	@mkdir -p $(DISTDIR)/$(SHAREDIR)
	@cp -r $ $(SHAREDIR)/hints.js $(DISTDIR)/$(SHAREDIR)
	@mkdir -p $(DISTDIR)/$(SRCDIR)
	@cp -r $ $(SOURCE) $(HDR) $(SRCDIR)/Makefile $(SRCDIR)/config.h $(DISTDIR)/$(SRCDIR)
	@tar cfz $(DISTDIR).tar.gz $(DISTDIR)
	@rm -rf  $(DISTDIR)


.PHONY: clean all install uninstall dist  distclean
