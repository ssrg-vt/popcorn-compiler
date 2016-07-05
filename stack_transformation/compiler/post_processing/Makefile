BUILD := build
BIN   := gen-stackinfo
UTIL  := dump-stackinfo

POPCORN        := /usr/local/popcorn
POPCORN_X86_64 := $(POPCORN)/x86_64
COMMON         := $(shell readlink -f ../../common)

CC     := gcc
CFLAGS := -O3 -Wall -Iinclude -I $(COMMON)/include \
          -specs $(POPCORN_X86_64)/lib/musl-gcc.specs
LIB    := -lelf

HDR := $(shell ls include/*.h) $(shell ls $(COMMON)/include/*.h)
SRC := $(shell ls src/*.c)
OBJ := $(subst src,$(BUILD),$(SRC:.c=.o))

UTIL_OBJ := $(BUILD)/bin.o $(BUILD)/util.o $(BUILD)/strings.o

all: $(BIN)

%/.dir:
	@echo " [MK] $*"
	@mkdir -p $*
	@touch $@

build/%.o: src/%.c $(HDR)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -o $@ -c $<

$(BIN): $(BUILD)/.dir $(OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LIB)

util: $(UTIL)

$(UTIL): utils/dump_metadata.c $(UTIL_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(UTIL) $< $(UTIL_OBJ) $(LIB)

install: $(BIN) $(UTIL)
	@echo " [INSTALL] $^ to $(POPCORN)/bin"
	@cp $^ $(POPCORN)/bin

clean:
	@echo " [RM] $(BIN) $(BUILD) $(UTIL)"
	@rm -rf $(BIN) $(BUILD) $(UTIL)

.PHONY: all util clean
