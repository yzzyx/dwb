NAME=dwb
VERSION=0.0.17
DISTDIR=$(NAME)-$(VERSION)
DOCDIR=doc
SRCDIR=src
SHAREDIR=data
EXAMPLEDIR=examples


LIBS  += gtk+-2.0
LIBS  += webkit-1.0

FLAGS += -pedantic
FLAGS += -Wall 
FLAGS += -pipe
FLAGS += `pkg-config --cflags --libs $(LIBS)` 
FLAGS +=--ansi
FLAGS +=-std=c99
FLAGS +=-D_POSIX_SOURCE
FLAGS +=-D_BSD_SOURCE


DFLAGS += -g -B


OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
SOURCE = $(wildcard $(SRCDIR)/*.c) 
HDR = $(SOURCE:%.c=%.h) 
TARGET = $(NAME)
DTARGET=$(TARGET)_d

EXAMPLES += $(EXAMPLEDIR)/ext_editor.sh 
EXAMPLES += $(EXAMPLEDIR)/formfiller.sh

MANFILE=$(NAME).1

MAKE=make --no-print-directory


BUILDDATE=`date +%Y.%m.%d`

#directories
PREFIX=/usr/

BINDIR=$(PREFIX)/bin

DATAROOTDIR=$(PREFIX)/share
DATADIR=$(DATAROOTDIR)

MANDIR=$(DATAROOTDIR)/man
MAN1DIR=$(MANDIR)/man1
