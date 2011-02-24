include ../config.mk

# needed for migration to gtk3
# FLAGS+=-DGTK_DISABLE_SINGLE_INCLUDES
# FLAGS+=-DGTK_DISABLE_DEPRECATED
# FLAGS+=-DGDK_DISABLE_DEPRECATED
# FLAGS+=-DGSEAL_ENABLE

all: $(TARGET) 

$(TARGET): $(OBJ)
	@echo "$(CC) $@"
	@$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET) 

-include $(OBJ:.o=.d)

%.o: %.c 
	@echo "${CC} $<"
	@$(CC) $(FLAGS) $(DEBUG) -c -o $@ $<
	@$(CC) $(FLAGS) -MM $*.c > $*.d


debug: 
	@$(MAKE) -B DEBUG=$(DFLAGS) TARGET=$(DTARGET)

cgdb: debug
	cgdb $(DTARGET)

clean:
	@echo Cleaning
	@$(RM) *.o 
	@$(RM) $(TARGET)
	@$(RM) $(DTARGET)
	@$(RM) *.d

.PHONY: clean all
