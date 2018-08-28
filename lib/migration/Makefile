POPCORN         ?= $(HOME)/hermit-popcorn/
POPCORN_ARM     := $(POPCORN)/aarch64-hermit
POPCORN_X86     := $(POPCORN)/x86_64-hermit

# Although we disable IR optimizations, use -O3 to get backend optimizations
CC         := $(POPCORN)/x86_64-host/bin/clang
CFLAGS     := -O0 -mllvm -optimize-regalloc -Wall -c -g -popcorn-alignment \
              -Wno-unused-variable -D_GNU_SOURCE -mllvm -no-sm-warn -nostdlib \
			  -nostdinc

#ifneq ($(findstring env_select,$(type)),)
#CFLAGS     += -D_ENV_SELECT_MIGRATE=1
#endif
#ifneq ($(findstring native,$(type)),)
#CFLAGS     += -D_NATIVE=1
#endif
ifneq ($(findstring debug,$(type)),)
CFLAGS     += -D_DEBUG=1
endif
#ifneq ($(findstring timing,$(type)),)
#CFLAGS     += -D_TIME_REWRITE=1 -D_TIME_RESPONSE_DELAY=1
#endif
#ifneq ($(findstring signal_trigger,$(type)),)
#CFLAGS     += -D_SIG_MIGRATION=1
#endif

CFLAGS_ARM     := $(CFLAGS) -target aarch64-hermit
CFLAGS_X86     := $(CFLAGS) -target x86_64-hermit

COMMON      := $(shell readlink -f ../../common/include)
INC         := -I./include -I$(COMMON) -isystem $(POPCORN)/x86_64-host/lib/clang/3.7.1/include
INC_ARM     := $(INC) -isystem $(POPCORN_ARM)/include
INC_X86     := $(INC) -isystem $(POPCORN_X86)/include

OPT       := $(POPCORN)/x86_64-host/bin/opt
OPT_FLAGS := -disable-loop-vectorization -disable-slp-vectorization \
             -insert-stackmaps -no-live-vals

AR      := $(POPCORN)/x86_64-host/bin/x86_64-hermit-ar
ARFLAGS := -cq

HDR := $(shell ls include/*.h)
SRC := $(shell ls src/*.c)

BUILD         := build
BUILD_ARM     := $(BUILD)/aarch64
BUILD_X86     := $(BUILD)/x86_64

HDR_ARM     := $(HDR) $(shell ls include/arch/aarch64/*.h)
HDR_X86     := $(HDR) $(shell ls include/arch/x86_64/*.h)

OBJ         := $(SRC:.c=.o)
OBJ_ARM     := $(subst src,$(BUILD_ARM),$(OBJ))
OBJ_X86     := $(subst src,$(BUILD_X86),$(OBJ))

LIB         := libmigrate.a
LIB_ARM     := $(BUILD_ARM)/$(LIB)
LIB_X86     := $(BUILD_X86)/$(LIB)

all: $(LIB_ARM) $(LIB_X86)

%/.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

$(BUILD_ARM)/%.o: src/%.c $(HDR_ARM)
	@echo " [CC] $< (aarch64)"
	@$(CC) $(CFLAGS_ARM) $(INC_ARM) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CC) $(CFLAGS_ARM) -o $@ $@.bc

$(LIB_ARM): $(BUILD_ARM)/.dir $(HDR_ARM) $(OBJ_ARM)
	@echo " [AR] $(LIB_ARM)"
	@rm -f $(LIB_ARM)
	@$(AR) $(ARFLAGS) $(LIB_ARM) $(OBJ_ARM) &> /dev/null

$(BUILD_X86)/%.o: src/%.c $(HDR_X86)
	@echo " [CC] $< (x86-64)"
	@$(CC) $(CFLAGS_X86) $(INC_X86) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CC) $(CFLAGS_X86) -o $@ $@.bc

$(LIB_X86): $(BUILD_X86)/.dir $(HDR_X86) $(OBJ_X86)
	@echo " [AR] $(LIB_X86)"
	@rm -f $(LIB_X86)
	@$(AR) $(ARFLAGS) $(LIB_X86) $(OBJ_X86) &> /dev/null

install: $(LIB_ARM) $(LIB_X86)
	@echo " [INSTALL] $(LIB_ARM) to $(POPCORN_ARM)/lib"
	@cp $(LIB_ARM) $(POPCORN_ARM)/lib
	@cp $(HDR) $(POPCORN_ARM)/include
	@echo " [INSTALL] $(LIB_X86) to $(POPCORN_X86)/lib"
	@cp $(LIB_X86) $(POPCORN_X86)/lib
	@cp $(HDR) $(POPCORN_X86)/include

clean:
	@echo " [RM] $(BUILD)"
	@rm -rf $(BUILD)

.PHONY: all clean
