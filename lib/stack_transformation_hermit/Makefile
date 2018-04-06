###############################################################################
# Common
###############################################################################

HERMIT_TOOLCHAIN=/opt/hermit/
HERMIT_PREFIX=/home/usr/hermit/

POPCORN := /usr/local/popcorn
COMMON  := $(shell readlink -f ../../common)

BUILD := build
INC   := include
SRC   := src
UTILS := utils

CC     := $(POPCORN)/bin/clang
CFLAGS := -g -Wall -D_GNU_SOURCE --target=x86_64-hermit -popcorn-alignment \
	-nostdlib -nostdinc

AR  := $(HERMIT_TOOLCHAIN)/bin/x86_64-hermit-ar
LOC := -I$(INC) -I$(COMMON)/include -isystem $(HERMIT_PREFIX)/x86_64-hermit/include -isystem /usr/local/popcorn/bin/../lib/clang/3.7.1/include

LIB_HDR      := $(shell ls $(INC)/*.h)
LIB_SRC      := $(shell ls $(SRC)/*.c)
LIB_ARCH_SRC := $(shell ls $(SRC)/arch/aarch64/*.c) \
                $(shell ls $(SRC)/arch/powerpc64/*.c) \
                $(shell ls $(SRC)/arch/x86_64/*.c)

ifeq ($(type),debug)
CFLAGS += -O0 -mllvm -optimize-regalloc -D_DEBUG -D_CHECKS -D_LOG
else
CFLAGS += -O3
endif

###############################################################################
# x86-64
###############################################################################

POPCORN_X86_64 := $(POPCORN)/x86_64

BUILD_X86_64 := $(BUILD)/x86_64
LIB_X86_64   := $(BUILD_X86_64)/libstack-transform.a
CC_X86_64    := $(CC) -target x86_64-hermit
LOC_X86_64   := $(LOC)

#-isystem $(POPCORN_X86_64)/include

LIB_OBJ_X86_64      := $(subst $(SRC),$(BUILD_X86_64),$(LIB_SRC:.c=.o))
LIB_ARCH_OBJ_X86_64 := $(subst $(SRC),$(BUILD_X86_64),$(LIB_ARCH_SRC:.c=.o))

###############################################################################
# Recipes
###############################################################################

# Common

all: $(LIB_X86_64)

clean:
	@rm -rf $(BUILD) $(LIB_X86_64)

%/.dir:
	@echo " [MK] $*"
	@mkdir -p $*
	@touch $@

# x86-64

build/x86_64/arch/aarch64/%.o: src/arch/aarch64/%.c $(LIB_HDR)
	@echo " [CC-x86_64] $<"
	@$(CC_X86_64) $(CFLAGS) $(LOC_X86_64) -o $@ -c $<

build/x86_64/arch/powerpc64/%.o: src/arch/powerpc64/%.c $(LIB_HDR)
	@echo " [CC-x86_64] $<"
	@$(CC_X86_64) $(CFLAGS) $(LOC_X86_64) -o $@ -c $<

build/x86_64/arch/x86_64/%.o: src/arch/x86_64/%.c $(LIB_HDR)
	@echo " [CC-x86_64] $<"
	@$(CC_X86_64) $(CFLAGS) $(LOC_X86_64) -o $@ -c $<

build/x86_64/%.o: src/%.c $(LIB_HDR)
	@echo " [CC-x86_64] $<"
	@$(CC_X86_64) $(CFLAGS) $(LOC_X86_64) -o $@ -c $<

$(LIB_X86_64): $(BUILD_X86_64)/arch/aarch64/.dir \
               $(BUILD_X86_64)/arch/powerpc64/.dir \
               $(BUILD_X86_64)/arch/x86_64/.dir \
                $(LIB_OBJ_X86_64) \
                $(LIB_ARCH_OBJ_X86_64)
	@echo " [AR-x86_64] $@"
	@rm -f $(LIB_X86_64)
	@$(AR) -cq $(LIB_X86_64) $(LIB_OBJ_X86_64) $(LIB_ARCH_OBJ_X86_64)

install-x86_64: $(LIB_X86_64)
	@echo " [INSTALL] $< to $(POPCORN_X86_64)/lib"
	@cp $< $(POPCORN_X86_64)/lib
	@echo " [INSTALL] $(INC)/stack_transform.h to $(POPCORN_X86_64)/include"
	@cp $(INC)/stack_transform.h $(POPCORN_X86_64)/include
	@echo " [INSTALL] $(COMMON)/include/arch.h to $(POPCORN_X86_64)/include"
	@cp $(COMMON)/include/arch.h $(POPCORN_X86_64)/include
	@mkdir -p $(POPCORN_X86_64)/include/arch/aarch64
	@mkdir -p $(POPCORN_X86_64)/include/arch/powerpc64
	@mkdir -p $(POPCORN_X86_64)/include/arch/x86_64
	@cp $(INC)/arch/aarch64/regs.h $(POPCORN_X86_64)/include/arch/aarch64
	@cp $(INC)/arch/powerpc64/regs.h $(POPCORN_X86_64)/include/arch/powerpc64
	@cp $(INC)/arch/x86_64/regs.h $(POPCORN_X86_64)/include/arch/x86_64

# install-powerpc64
install: install-aarch64 install-x86_64

.PHONY: all install install-aarch64 install-powerpc64 install-x86_64 clean
