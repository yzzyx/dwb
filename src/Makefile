include ../config.mk

all: $(TARGET) 

$(TARGET): $(OBJ)
	@echo "$(CC) $@"
	@$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET) 

-include $(OBJ:.o=.d)
-include $(DOBJ:.do=.dd)

%.o: %.c 
	@echo "${CC} $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

#@$(CC) $(FLAGS) -MM $*.c > $*.d

debug: $(DTARGET)

%.do: %.c
	@echo "${CC} $<"
	@$(CC) $(DCFLAGS) -c -o $@ $<

$(DTARGET): $(DOBJ) 
	@echo "$(CC) $@"
	@$(CC) $(LDFLAGS) $(DOBJ) -o $(DTARGET) 

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
