include ../config.mk

%.o: %.c $(HDR)
	@echo "${CC} $<"
	@$(CC) $(FLAGS) $(DEBUG) -c -o $@ $<

all: $(TARGET) 

$(TARGET): $(OBJ) 
	@$(CC) $(FLAGS) $(OBJ) -o $(TARGET) 

debug: 
	@$(MAKE) -B DEBUG=$(DFLAGS) TARGET=$(DTARGET)

cgdb: debug
	cgdb $(DTARGET)

clean:
	@echo Cleaning
	@$(RM) $(OBJ)
	@$(RM) $(TARGET)
	@$(RM) $(DTARGET)

.PHONY: clean all
