BUILD		:= build
BIN		:= gen-stackinfo
DUMP_SM	:= dump-llvm-stackmap
DUMP_ST	:= dump-stackinfo
CHECK		:= check-stackmaps

POPCORN        := /usr/local/popcorn
POPCORN_X86_64 := $(POPCORN)/x86_64
COMMON         := $(shell readlink -f ../../common/stack_transformation)

CC     := gcc
CFLAGS := -O3 -g -Wall -Iinclude -I$(COMMON)/include -Iext/include -Lext/lib/
          
#-specs $(POPCORN_X86_64)/lib/musl-gcc.specs
LIB    := -lelf

HDR := $(shell ls include/*.h) $(shell ls $(COMMON)/include/*.h)
SRC := $(shell ls src/*.c)
OBJ := $(subst src,$(BUILD),$(SRC:.c=.o))

UTIL_OBJ  := $(BUILD)/bin.o $(BUILD)/util.o $(BUILD)/strings.o
CHECK_OBJ := $(UTIL_OBJ) $(BUILD)/stackmap.o

all: $(BIN) $(DUMP_SM) $(DUMP_ST) $(CHECK)

%/.dir:
	@echo " [MK] $*"
	@mkdir -p $*
	@touch $@

build/%.o: src/%.c $(HDR)
	@echo " [CC] $<"
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN): $(BUILD)/.dir $(OBJ)
	@echo " [CC] $@"
	$(CC) $(CFLAGS) -o $(@) $(OBJ) $(LIB)

util: $(DUMP_SM) $(DUMP_ST)

$(DUMP_SM): utils/dump_llvm_stackmap.c $(CHECK_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $^ $(LIB)

$(DUMP_ST): utils/dump_stackinfo.c $(UTIL_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $^ $(LIB)

check: $(CHECK)

$(CHECK): utils/check_stackmaps.c $(CHECK_OBJ)
	@echo " [CC] $@"
	@$(CC) $(CFLAGS) -o $(@) $^ $(LIB)

install: $(BIN) $(DUMP_SM) $(DUMP_ST) $(CHECK)
	@echo " [INSTALL] $^ to $(POPCORN)/bin"
	@cp $^ $(POPCORN)/bin

clean:
	@echo " [RM] $(BIN) $(BUILD) $(DUMP_SM) $(DUMP_ST) $(CHECK)"
	@rm -rf $(BIN) $(BUILD) $(DUMP_SM) $(DUMP_ST) $(CHECK)

.PHONY: all util clean
