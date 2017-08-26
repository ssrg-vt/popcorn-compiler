POPCORN := /usr/local/popcorn

BIN    := align
JAR    := src/align.jar
SCRIPT := align.script

all: $(BIN)

#$(JAR):
.PHONY: jar
jar:
	@make -C src

$(BIN): jar #$(JAR)
	@echo " [MAKE] $@"
	@cat $(SCRIPT) $(JAR) > $(BIN)
	@chmod +x $(BIN)

install: $(BIN)
	@echo " [INSTALL] $(BIN) to $(POPCORN)/bin"
	@cp $(BIN) $(POPCORN)/bin
	@echo " [INSTALL] scripts/ to $(POPCORN)/lib"
	@cp -r scripts $(POPCORN)/lib

clean:
	@echo " [RM] $(BIN)"
	@make -C src clean
	@rm -f $(BIN)

.PHONY: all install clean
