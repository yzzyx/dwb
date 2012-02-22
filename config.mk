REAL_NAME=dwb
REAL_VERSION=0.1.0
COPYRIGHT="© 2011 portix"

# dirs
DISTDIR=$(REAL_NAME)-$(REAL_VERSION)
DOCDIR=doc
SRCDIR=src
JSDIR=scripts
LIBDIR=lib
SHAREDIR=share
UTILDIR=util
SUBDIRS=$(SRCDIR) $(UTILDIR) 

# Version info
HG_VERSION=$(shell hg id -n 2>/dev/null)
VERSION=$(shell if [ $(HG_VERSION) ]; then echo "rev.\ $(HG_VERSION)"; else echo "$(REAL_VERSION)"; fi)
NAME=$(shell if [ $(HG_VERSION) ]; then echo "$(REAL_NAME)-hg"; else echo "$(REAL_NAME)"; fi)
BUILDDATE=`date +%Y.%m.%d`

# Targets
TARGET = $(REAL_NAME)
DTARGET=$(TARGET)_d


# target directories
PREFIX=/usr
BINDIR=$(PREFIX)/bin
DATAROOTDIR=$(PREFIX)/share
DATADIR=$(DATAROOTDIR)

# manpages
MANFILE=$(REAL_NAME).1
MANDIR=$(DATAROOTDIR)/man
MAN1DIR=$(MANDIR)/man1

# Compiler
CC ?= gcc

GTK3LIBS=gtk+-3.0 webkitgtk-3.0 
ifeq ($(shell pkg-config --exists javascriptcoregtk-3.0 && echo 1), 1)
GTK3LIBS+=javascriptcoregtk-3.0
endif
GTK2LIBS=gtk+-2.0 webkit-1.0 
ifeq ($(shell pkg-config --exists javascriptcoregtk-1.0 && echo 1), 1)
GTK2LIBS+=javascriptcoregtk-1.0
endif

LIBSOUP=libsoup-2.4

ifeq ($(shell pkg-config --exists $(LIBSOUP) && echo 1), 1)
LIBS=$(LIBSOUP)
else
$(error Cannot find $(LIBSOUP))
endif

#determine gtk version
ifeq (${GTK}, 3) 																							#GTK=3
ifeq ($(shell pkg-config --exists $(GTK3LIBS) && echo 1), 1) 		#has gtk3 libs
LIBS+=$(GTK3LIBS)
USEGTK3 := 1
else 																														#has gtk3 libs
ifeq ($(shell pkg-config --exists $(GTK2LIBS) && echo 1), 1) 			#has gtk2 libs
$(warning Cannot find gtk3-libs, falling back to gtk2)
LIBS+=$(GTK2LIBS)
else 																															#has gtk2 libs
$(error Cannot find gtk2-libs or gtk3-libs)
endif 																														#has gtk2 libs
endif 																													#has gtk3 libs
else 																													#GTK=3
ifeq ($(shell pkg-config --exists $(GTK2LIBS) && echo 1), 1)		#has gtk2 libs
LIBS+=$(GTK2LIBS)
else 																														#has gtk2 libs
ifeq ($(shell pkg-config --exists $(GTK3LIBS) && echo 1), 1) 			#has gtk3 libs
LIBS+=$(GTK3LIBS)
USEGTK3 := 1
$(warning Cannot find gtk2-libs, falling back to gtk3)
else 																															#has gtk3 libs
$(error Cannot find gtk2-libs or gtk3-libs)
endif 																														#has gtk3 libs
endif 																													#has gtk2 libs
endif 																												#GTK=3



# HTML-files
INFO_FILE=info.html
SETTINGS_FILE=settings.html
HEAD_FILE=head.html
KEY_FILE=keys.html
ERROR_FILE=error.html
LOCAL_FILE=local.html

# VARIOUS FILES
PLUGIN_FILE=pluginblocker.asc

# CFLAGS
CFLAGS := $(CFLAGS)
CFLAGS += -Wall
CFLAGS += -pipe
CFLAGS += $(shell pkg-config --cflags $(LIBS))
CFLAGS += --ansi
CFLAGS += -std=c99
CFLAGS += -D_POSIX_SOURCE
CFLAGS += -O2
CFLAGS += -g
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_NETBSD_SOURCE
#defines
CFLAGS += -DNAME=\"$(NAME)\" 
CFLAGS += -DVERSION=\"$(VERSION)\" 
CFLAGS += -DCOPYRIGHT=\"$(COPYRIGHT)\"
CFLAGS += -DREAL_NAME=\"$(REAL_NAME)\"
CFLAGS += -DPLUGIN_FILE=\"$(PLUGIN_FILE)\"
CFLAGS += -DINFO_FILE=\"$(INFO_FILE)\"
CFLAGS += -DSETTINGS_FILE=\"$(SETTINGS_FILE)\"
CFLAGS += -DHEAD_FILE=\"$(HEAD_FILE)\"
CFLAGS += -DKEY_FILE=\"$(KEY_FILE)\"
CFLAGS += -DERROR_FILE=\"$(ERROR_FILE)\"
CFLAGS += -DLOCAL_FILE=\"$(LOCAL_FILE)\"
CFLAGS += -DSYSTEM_DATA_DIR=\"$(DATADIR)\"

# If execinfo.h is not available, e.g. freebsd
ifneq (${WITHOUT_EXECINFO}, 1)
CFLAGS += -DHAS_EXECINFO
endif

ifeq (USEGTK3, 1) 
CFLAGS+=-DGTK_DISABLE_SINGLE_INCLUDES
CFLAGS+=-DGTK_DISABLE_DEPRECATED
CFLAGS+=-DGDK_DISABLE_DEPRECATED
CFLAGS+=-DGSEAL_ENABLE
endif
CFLAGS +=-I/usr/lib/dwb/ 

# LDFLAGS
LDFLAGS = $(shell pkg-config --libs $(LIBS)) -rdynamic

# Debug flags
DCFLAGS = $(CFLAGS)
DCFLAGS += -DDWB_DEBUG
DCFLAGS += -g 
DCFLAGS += -O0 

# Makeflags
MFLAGS=--no-print-directory

#Input 
SOURCE = $(wildcard *.c) 
HDR = $(wildcard *.h) 

# OUTPUT 
# Objects
OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
DOBJ = $(patsubst %.c, %.do, $(wildcard *.c))

