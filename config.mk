NAME=dwb
VERSION=0.0.16
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


#directories
PREFIX=/usr/

BINDIR=$(PREFIX)/bin

DATAROOTDIR=$(PREFIX)/share
DATADIR=$(DATAROOTDIR)

MANDIR=$(DATAROOTDIR)/man
MAN1DIR=$(MANDIR)/man1
