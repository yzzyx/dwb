include ../config.mk

all: $(TARGET) 

$(TARGET): $(OBJ)
	@echo "$(CC) $@"
	@$(CC) $(LDFLAGS) $(OBJ) -o $(TARGET) 

-include $(OBJ:.o=.d)
-include $(DOBJ:.do=.dd)

CFLAGS+=-I./lib/

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

tlds.h: tlds.in
	@echo gen tlds.h
	@echo "#ifndef TLDS_H" > $@
	@echo "#define TLDS_H" >> $@
	@echo "static char *TLDS_EFFECTIVE[] = {" >> $@
	@sed '/^\/\/\|^\s*$$/d;s/^/\t"/;s/$$/",/' $< >> $@
	@echo "NULL," >> $@
	@echo "};" >> $@
	@echo "#endif" >> $@

domain.o: tlds.h

domain.do: tlds.h

#
#tlds.h: effective_tld_names.dat
#	@echo gen tlds.h
#	@echo "#ifndef TLDS_H" > $@
#	@echo "#define TLDS_H" >> $@
#	@echo "static char *effective_tlds[] = {" >> $@
#	@sed '/^\/\/\|^\s*$$/d;s/\(.*\)/\t"\1",/' $< >> $@
#	@echo -e "\tNULL," >> $@
#	@echo "};" >> $@
#	@echo "#endif" >> $@


cgdb: debug
	cgdb $(DTARGET)

clean:
	$(RM) *.o  *.do $(TARGET) $(DTARGET) *.d
	$(RM) tlds.h

.PHONY: clean all
