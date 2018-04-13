###############################################################################
# Common
###############################################################################

POPCORN ?= $(HOME)/hermit-popcorn/
COMMON  := $(shell readlink -f ../../common)

BUILD := build
INC   := include
SRC   := src
UTILS := utils

CC     := $(POPCORN)/x86_64-host/bin/clang
CFLAGS := -g -Wall -D_GNU_SOURCE -popcorn-alignment -I$(POPCORN)/x86_64-host/include

AR  := ar
LOC := -I$(INC) -I$(COMMON)/include

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
# aarch64
###############################################################################

POPCORN_AARCH64 := $(POPCORN)/aarch64

BUILD_AARCH64 := $(BUILD)/aarch64
LIB_AARCH64   := $(BUILD_AARCH64)/libstack-transform.a
CC_AARCH64    := $(CC) -target aarch64-linux-gnu
LOC_AARCH64   := $(LOC) -isystem $(POPCORN_AARCH64)/include

LIB_OBJ_AARCH64      := $(subst $(SRC),$(BUILD_AARCH64),$(LIB_SRC:.c=.o))
LIB_ARCH_OBJ_AARCH64 := $(subst $(SRC),$(BUILD_AARCH64),$(LIB_ARCH_SRC:.c=.o))

###############################################################################
# x86-64
###############################################################################

POPCORN_X86_64 := $(POPCORN)/x86_64

BUILD_X86_64 := $(BUILD)/x86_64
LIB_X86_64   := $(BUILD_X86_64)/libstack-transform.a
#CC_X86_64    := $(CC) -target x86_64-linux-gnu
CC_X86_64    := $(CC) -target x86_64-hermit
#LOC_X86_64   := $(LOC) -isystem $(POPCORN_X86_64)/include
LOC_X86_64   := $(LOC) -isystem /home/usr/hermit/x86_64-hermit/include

LIB_OBJ_X86_64      := $(subst $(SRC),$(BUILD_X86_64),$(LIB_SRC:.c=.o))
LIB_ARCH_OBJ_X86_64 := $(subst $(SRC),$(BUILD_X86_64),$(LIB_ARCH_SRC:.c=.o))

###############################################################################
# Recipes
###############################################################################

# Common

# $(LIB_POWERPC64)
#all: $(LIB_AARCH64) $(LIB_X86_64)
all: $(LIB_X86_64)

clean:
	@echo " [RM] $(BUILD)"
	@echo " [RM] $(LIB_AARCH64)"
	@echo " [RM] $(LIB_X86_64)"
	@rm -rf $(BUILD) $(LIB_AARCH64) $(LIB_X86_64)

%/.dir:
	@echo " [MK] $*"
	@mkdir -p $*
	@touch $@

# aarch64

build/aarch64/arch/aarch64/%.o: src/arch/aarch64/%.c $(LIB_HDR)
	@echo " [CC-aarch64] $<"
	@$(CC_AARCH64) $(CFLAGS) $(LOC_AARCH64) -o $@ -c $<

build/aarch64/arch/powerpc64/%.o: src/arch/powerpc64/%.c $(LIB_HDR)
	@echo " [CC-aarch64] $<"
	@$(CC_AARCH64) $(CFLAGS) $(LOC_AARCH64) -o $@ -c $<

build/aarch64/arch/x86_64/%.o: src/arch/x86_64/%.c $(LIB_HDR)
	@echo " [CC-aarch64] $<"
	@$(CC_AARCH64) $(CFLAGS) $(LOC_AARCH64) -o $@ -c $<

build/aarch64/%.o: src/%.c $(LIB_HDR)
	@echo " [CC-aarch64] $<"
	@$(CC_AARCH64) $(CFLAGS) $(LOC_AARCH64) -o $@ -c $<

$(LIB_AARCH64): $(BUILD_AARCH64)/arch/aarch64/.dir \
                $(BUILD_AARCH64)/arch/powerpc64/.dir \
                $(BUILD_AARCH64)/arch/x86_64/.dir \
                $(LIB_OBJ_AARCH64) \
                $(LIB_ARCH_OBJ_AARCH64)
	@echo " [AR-aarch64] $@"
	@rm -f $(LIB_AARCH64)
	@$(AR) -cq $(LIB_AARCH64) $(LIB_OBJ_AARCH64) $(LIB_ARCH_OBJ_AARCH64)

install-aarch64: $(LIB_AARCH64)
	@echo " [INSTALL] $< to $(POPCORN_AARCH64)/lib"
	@cp $< $(POPCORN_AARCH64)/lib
	@echo " [INSTALL] $(INC)/stack_transform.h to $(POPCORN_AARCH64)/include"
	@cp $(INC)/stack_transform.h $(POPCORN_AARCH64)/include
	@echo " [INSTALL] $(COMMON)/include/arch.h to $(POPCORN_AARCH64)/include"
	@cp $(COMMON)/include/arch.h $(POPCORN_AARCH64)/include
	@mkdir -p $(POPCORN_AARCH64)/include/arch/aarch64
	@mkdir -p $(POPCORN_AARCH64)/include/arch/powerpc64
	@mkdir -p $(POPCORN_AARCH64)/include/arch/x86_64
	@cp $(INC)/arch/aarch64/regs.h $(POPCORN_AARCH64)/include/arch/aarch64
	@cp $(INC)/arch/powerpc64/regs.h $(POPCORN_AARCH64)/include/arch/powerpc64
	@cp $(INC)/arch/x86_64/regs.h $(POPCORN_AARCH64)/include/arch/x86_64

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

#modified by mehrab
POPCORN_X86_64 = "/home/usr/hermit/x86_64-hermit"
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

#install: install-aarch64 install-x86_64
install: install-x86_64

.PHONY: all install install-aarch64 install-x86_64 clean
