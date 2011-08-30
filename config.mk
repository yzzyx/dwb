REAL_NAME=dwb
REAL_VERSION=0.0.18
COPYRIGHT="© 2011 Stefan Bolte"
DISTDIR=$(REAL_NAME)-$(REAL_VERSION)
DOCDIR=doc
SRCDIR=src
JSDIR=scripts
EXAMPLEDIR=examples
LIBDIR=lib
SHAREDIR=share

# VARIOUS FILES
PLUGIN_FILE=pluginblocker.asc


HG_VERSION=$(shell hg id -n 2>/dev/null)

VERSION=$(shell if [ $(HG_VERSION) ]; then echo "rev.\ $(HG_VERSION)"; else echo "$(REAL_VERSION)"; fi)
NAME=$(shell if [ $(HG_VERSION) ]; then echo "$(REAL_NAME)-hg"; else echo "$(REAL_NAME)"; fi)

##ifeq (${GTK}, 3)
##LIBS  += gtk+-3.0
##LIBS  += webkitgtk-3.0
##else
##LIBS  += gtk+-2.0
##LIBS  += webkit-1.0
##endif
ifeq (${GTK}, 3)
LIBS  += gtk+-3.0
LIBS  += webkitgtk-3.0
FLAGS+=-DGTK_DISABLE_SINGLE_INCLUDES
FLAGS+=-DGTK_DISABLE_DEPRECATED
FLAGS+=-DGDK_DISABLE_DEPRECATED
FLAGS+=-DGSEAL_ENABLE
else
LIBS  += gtk+-2.0
LIBS  += webkit-1.0
endif
#LIBS  += json-glib-1.0

INFO_FILE=info.html
SETTINGS_FILE=settings.html
HEAD_FILE=head.html
KEY_FILE=keys.html

#FLAGS += -pedantic
FLAGS += -Wall 
FLAGS += -pipe
FLAGS += `pkg-config --cflags --libs $(LIBS)` 
FLAGS += --ansi
FLAGS += -std=c99
FLAGS += -D_POSIX_SOURCE
FLAGS += -D_BSD_SOURCE
FLAGS += -DNAME=\"$(NAME)\" 
FLAGS += -DVERSION=\"$(VERSION)\" 
FLAGS += -DCOPYRIGHT=\"$(COPYRIGHT)\"
FLAGS += -DREAL_NAME=\"$(REAL_NAME)\"
FLAGS += -DPLUGIN_FILE=\"$(PLUGIN_FILE)\"

FLAGS += -DINFO_FILE=\"$(INFO_FILE)\"
FLAGS += -DSETTINGS_FILE=\"$(SETTINGS_FILE)\"
FLAGS += -DHEAD_FILE=\"$(HEAD_FILE)\"
FLAGS += -DKEY_FILE=\"$(KEY_FILE)\"

DFLAGS += $(FLAGS)
DFLAGS += -DDWB_DEBUG
DFLAGS += -g 


OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
DOBJ = $(patsubst %.c, %.do, $(wildcard *.c))
SOURCE = $(wildcard $(SRCDIR)/*.c) 
HDR = $(SOURCE:%.c=%.h) 
TARGET = $(REAL_NAME)
DTARGET=$(TARGET)_d

EXAMPLES += $(EXAMPLEDIR)/ext_editor.sh 
EXAMPLES += $(EXAMPLEDIR)/formfiller.sh

MANFILE=$(REAL_NAME).1

MAKE=make --no-print-directory


BUILDDATE=`date +%Y.%m.%d`

#directories
PREFIX=/usr

BINDIR=$(PREFIX)/bin

DATAROOTDIR=$(PREFIX)/share
DATADIR=$(DATAROOTDIR)


MANDIR=$(DATAROOTDIR)/man
MAN1DIR=$(MANDIR)/man1
