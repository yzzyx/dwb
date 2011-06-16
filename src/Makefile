include ../config.mk

# needed for migration to gtk3
# FLAGS+=-DGTK_DISABLE_SINGLE_INCLUDES
# FLAGS+=-DGTK_DISABLE_DEPRECATED
# FLAGS+=-DGDK_DISABLE_DEPRECATED
# FLAGS+=-DGSEAL_ENABLE

all: $(TARGET) 

$(TARGET): $(OBJ)
	@echo "$(CC) $@"
	@$(CC) $(FLAGS) $(OBJ) -o $(TARGET) 

-include $(OBJ:.o=.d)
-include $(DOBJ:.do=.dd)

%.o: %.c 
	@echo "${CC} $<"
	@$(CC) $(FLAGS) -c -o $@ $<
#	@$(CC) $(FLAGS) -MM $*.c > $*.d


debug: $(DTARGET)

%.do: %.c
	@echo "${CC} $<"
	@$(CC) $(DFLAGS) -c -o $@ $<

$(DTARGET): $(DOBJ) 
	@echo "$(CC) $@"
	@$(CC) $(DFLAGS) $(DOBJ) -o $(DTARGET) 

cgdb: debug
	cgdb $(DTARGET)

clean:
	@echo Cleaning
	@$(RM) *.o  
	@$(RM) *.do  
	@$(RM) $(TARGET)
	@$(RM) $(DTARGET)
	@$(RM) *.d

.PHONY: clean all
