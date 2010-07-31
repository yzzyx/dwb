include config.mk

%.o: %.c $(HDR)
	@echo "${CC} $<"
	@$(CC) $(FLAGS) $(DEBUG) -c -o $@ $<

all: $(TARGET) 

$(TARGET): $(OBJ)
	@$(CC) $(FLAGS) $(OBJ) -o $(TARGET) 


debug: 
	@$(MAKE) -B DEBUG=$(DFLAGS) TARGET=$(DTARGET)

clean:
	@echo cleaning...
	@echo "removing $(OBJ) $(TARGET)"
	@$(RM) $(OBJ)
	@$(RM) $(TARGET)
	@$(RM) config.h hints.h startpage.h

.PHONY: clean all
