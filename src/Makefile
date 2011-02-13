include ../config.mk

# needed for migration to gtk3
# FLAGS+=-DGTK_DISABLE_SINGLE_INCLUDES
# FLAGS+=-DGTK_DISABLE_DEPRECATED
# FLAGS+=-DGDK_DISABLE_DEPRECATED
# FLAGS+=-DGSEAL_ENABLE

%.o: %.c $(HDR)
	@echo "${CC} $<"
	@$(CC) $(FLAGS) $(DEBUG) -c -o $@ $<

all: $(TARGET) 

$(TARGET): $(OBJ) $(HDR)
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
