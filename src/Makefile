include ../config.mk

DEPS=$(patsubst %.o, %.d, $(OBJ))
#all: $(TARGET) 

$(TARGET): $(OBJ)
	@echo "$(CC) $@"
	@$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS) 

-include $(OBJ:.o=.d)
-include $(DOBJ:.do=.dd)

%.o: %.c %.h
	@echo "${CC} $<"
	@$(CC) -c -o $@ $< $(CFLAGS) 

debug: $(DTARGET)

#%.d: %.c %.h
#	@echo "$(CC) -MM $@"
#	@$(CC) $(CFLAGS) -MM $< -o $@
#	

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
