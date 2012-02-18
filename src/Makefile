include ../config.mk

DEPS=$(patsubst %.o, %.d, $(OBJ))
#all: $(TARGET) 

$(TARGET): $(OBJ)
	@echo "$(CC) $@"
	@$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS) 

-include $(OBJ:.o=.d)
-include $(DOBJ:.do=.dd)

%.o: %.c %.h config.h dwb.h
	@echo "${CC} $<"
	@$(CC) -c -o $@ $< $(CFLAGS) 

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

tlds.h: tlds.in
	@echo gen tlds.h
	@echo "#ifndef TLDS_H" > $@
	@echo "#define TLDS_H" >> $@
	@echo "static char *TLDS_EFFECTIVE[] = {" >> $@
	@sed '/^\/\//d;/^[[:space:]]*$$/d;s/^/	"/;s/$$/",/' $< >> $@
	@echo "NULL," >> $@
	@echo "};" >> $@
	@echo "#endif" >> $@

domain.o: tlds.h

domain.do: tlds.h


dependencies: $(DEPS)

deps: 
	$(MAKE) -B dependencies


#
#tlds.h: effective_tld_names.dat
#	@echo gen tlds.h
#	@echo "#ifndef TLDS_H" > $@
#	@echo "#define TLDS_H" >> $@
#	@echo "static char *effective_tlds[] = {" >> $@
#	@sed '/^\/\//d;/^[[:space:]]*$$/d;s/\(.*\)/	"\1",/' $< >> $@
#	@echo -e "\tNULL," >> $@
#	@echo "};" >> $@
#	@echo "#endif" >> $@


cgdb: debug
	cgdb $(DTARGET)

clean:
	$(RM) *.o  *.do $(TARGET) $(DTARGET) *.d
	$(RM) tlds.h

.PHONY: clean all cgdb deps
