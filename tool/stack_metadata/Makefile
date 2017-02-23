BUILD := build
BIN   := gen-stackinfo
UTIL  := dump-stackinfo
CHECK := check-stackmaps

POPCORN        := /usr/local/popcorn
POPCORN_X86_64 := $(POPCORN)/x86_64
COMMON         := $(shell readlink -f ../../common/stack_transformation)

CC     := gcc
CFLAGS := -O3 -g -Wall -Iinclude -I$(COMMON)/include \
          -specs $(POPCORN_X86_64)/lib/musl-gcc.specs
LIB    := -lelf

HDR := $(shell ls include/*.h) $(shell ls $(COMMON)/include/*.h)
SRC := $(shell ls src/*.c)
OBJ := $(subst src,$(BUILD),$(SRC:.c=.o))

UTIL_OBJ  := $(BUILD)/bin.o $(BUILD)/util.o $(BUILD)/strings.o
CHECK_OBJ := $(UTIL_OBJ) $(BUILD)/stackmap.o

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

$(UTIL): utils/dump_stackinfo.c $(UTIL_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(UTIL) $^ $(LIB)

check: $(CHECK)

$(CHECK): utils/check_stackmaps.c $(CHECK_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(CHECK) $^ $(LIB)

install: $(BIN) $(UTIL) $(CHECK)
	@echo " [INSTALL] $^ to $(POPCORN)/bin"
	@cp $^ $(POPCORN)/bin

clean:
	@echo " [RM] $(BIN) $(BUILD) $(UTIL) $(CHECK)"
	@rm -rf $(BIN) $(BUILD) $(UTIL) $(CHECK)

.PHONY: all util clean
