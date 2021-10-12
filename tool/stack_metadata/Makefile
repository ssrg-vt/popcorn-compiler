BUILD		:= build
BIN		:= gen-stackinfo
DUMP_SM	:= dump-llvm-stackmap
DUMP_ST	:= dump-stackinfo
CHECK		:= check-stackmaps
COMPRESS	:= compress

POPCORN        := /usr/local/popcorn
POPCORN_X86_64 := $(POPCORN)/x86_64
COMMON         := $(shell readlink -f ../../common)

CC		:= x86_64-linux-gnu-gcc
CFLAGS		:= -O3 -g -Wall -Iinclude -I$(COMMON)/include \
		   -specs $(POPCORN_X86_64)/lib/musl-gcc.specs
LDFLAGS	:= -static
LIB    	:= -lelf

HDR := $(shell ls include/*.h) $(shell ls $(COMMON)/include/*.h)
SRC := $(shell ls src/*.c)
OBJ := $(subst src,$(BUILD),$(SRC:.c=.o))

UTIL_OBJ  := $(BUILD)/bin.o $(BUILD)/util.o $(BUILD)/strings.o
CHECK_OBJ := $(UTIL_OBJ) $(BUILD)/stackmap.o

all: $(BIN) util check

%/.dir:
	@echo " [MK] $*"
	@mkdir -p $*
	@touch $@

build/%.o: src/%.c $(HDR)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -o $@ -c $<

$(OBJ): $(BUILD)/.dir
$(UTIL_OBJ): $(BUILD)/.dir
$(CHECK_OBJ): $(BUILD)/.dir

$(BIN): $(OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $(OBJ) $(LDFLAGS) $(LIB)

util: $(DUMP_SM) $(DUMP_ST) $(COMPRESS)

$(DUMP_SM): utils/dump_llvm_stackmap.c $(CHECK_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $^ $(LDFLAGS) $(LIB)

$(DUMP_ST): utils/dump_stackinfo.c $(UTIL_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $^ $(LDFLAGS) $(LIB)

$(COMPRESS): utils/compress.c $(UTIL_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $^ $(LDFLAGS) $(LIB)

check: $(CHECK)

$(CHECK): utils/check_stackmaps.c $(CHECK_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $^ $(LDFLAGS) $(LIB)

install: $(BIN) $(DUMP_SM) $(DUMP_ST) $(CHECK) $(COMPRESS)
	@echo " [INSTALL] $^ to $(POPCORN)/bin"
	@cp $^ $(POPCORN)/bin

clean:
	@echo " [RM] $(BIN) $(BUILD) $(DUMP_SM) $(DUMP_ST) $(CHECK) $(COMPRESS)"
	@rm -rf $(BIN) $(BUILD) $(DUMP_SM) $(DUMP_ST) $(CHECK) $(COMPRESS)

.PHONY: all util clean
