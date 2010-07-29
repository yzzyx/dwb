CC    = gcc
LIBS  += gtk+-2.0
LIBS  += webkit-1.0
FLAGS += -Wall 
FLAGS += -pipe
FLAGS += `pkg-config --cflags --libs $(LIBS)` 

FLAGS +=--ansi
FLAGS +=-std=c99
#OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
SOURCE =  $(wildcard *.c)
HDR = config.h 
TARGET = dwb

%.o: %.c $(HDR)
	@echo "${CC} $<"
	@$(CC) $(FLAGS) $(DEBUG) -c -o $@ $<

all: $(TARGET) 

#hints.h: $(JAVASCRIPT)
#	@echo creating hints.h
#	@cat ../hintoptions.js > hints.h
#	@cat hints.js >> hints.h
#	@sed -i 's/\\/\\\\/g' hints.h
#	@sed -i 's/\"/\\\"/g' hints.h
#	@sed -i 's/\s\s*/ /g'  hints.h
#	@sed -i 's/^\s*\/\/.*//' hints.h 
#	@sed -i '1s/^/#define HINTS \"/' hints.h 
#	@sed -i ':a;N;s/\n//;ta' hints.h
#	@sed -i 's/$$/\"/' hints.h

$(TARGET): $(SOURCE) $(HDR)
	@$(CC) $(FLAGS) $(SOURCE) $(DEBUG) -o $(TARGET) 

debug: 
	@$(MAKE) -B DEBUG="-g" TARGET="dwb-debug"

clean:
	@echo cleaning
	@rm -f $(OBJ)
	@rm -f $(TARGET)
	@rm -f config.h hints.h startpage.h

.PHONY: clean all
