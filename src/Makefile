# See COPYING for copyright and license details

include ../config.mk

DEPS=$(patsubst %.o, %.d, $(OBJ))

all: $(TARGET)

$(TARGET): $(OBJ) 
	@echo "$(CC) $@"
	@$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS) 

-include $(OBJ:.o=.d)
-include $(DOBJ:.do=.dd)

%.o: %.c %.h config.h dwb.h tlds.h
	@echo "${CC} $<"
	@$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS) 

debug: $(DTARGET)

deps.d: %.c %.h tlds.h 
	@echo "$(CC) -MM $@"
	@$(CC) $(CFLAGS) -MM $< -o $@


%.do: %.c %.h config.h
	@echo "${CC} $<"
	@$(CC) -c -o $@ $< $(DCFLAGS) 

$(DTARGET): $(DOBJ) 
	@echo "$(CC) $@"
	@$(CC) $(DOBJ) -o $(DTARGET) $(LDFLAGS) 

tlds.h: tlds.in mktlds-header
	@echo gen tlds.h
	@./mktlds-header < tlds.in 

domain.o: tlds.h 

domain.do: tlds.h

dependencies: $(DEPS)

deps: 
	$(MAKE) -B dependencies

cgdb: debug
	cgdb $(DTARGET)

clean:
	$(RM) *.o  *.do $(TARGET) $(DTARGET) *.d
	$(RM) tlds.h

.PHONY: clean all cgdb deps 
