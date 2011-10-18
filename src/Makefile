include ../config.mk

all: $(TARGET) 

$(TARGET): $(OBJ)
	@echo "$(CC) $@"
	@$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS) 

-include $(OBJ:.o=.d)
-include $(DOBJ:.do=.dd)

%.o: %.c 
	@echo "${CC} $<"
	@$(CC) -c -o $@ $< $(CFLAGS) 

#@$(CC) $(FLAGS) -MM $*.c > $*.d

debug: $(DTARGET)

%.do: %.c
	@echo "${CC} $<"
	@$(CC) -c -o $@ $< $(DCFLAGS) 

$(DTARGET): $(DOBJ) 
	@echo "$(CC) $@"
	@$(CC) $(DOBJ) -o $(DTARGET) $(LDFLAGS) 

cgdb: debug
	cgdb $(DTARGET)

clean:
	$(RM) *.o  *.do $(TARGET) $(DTARGET) *.d

.PHONY: clean all
