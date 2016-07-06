###############################################################################
#                        System-specific locations                            #
###############################################################################

POPCORN := /usr/local/popcorn
ARM64_LIBGCC := /usr/local/gcc-cross/aarch64-linux-gnu/4.8

###############################################################################
#                  Compiler toolchain & command-line flags                    #
###############################################################################

# Compiler
CC     := clang
CFLAGS := -O3 -Wall -Wno-unused-variable -g -gdwarf-aranges \
          -nostdinc -nostdlib -static -I../../utils -DPOSIX
LIBS   := /lib/crt1.o \
          /lib/libc.a \
          /lib/libstack-transform.a \
          /lib/libdwarf.a \
          /lib/libelf.a \
          /lib/libc.a

OPT       := opt
OPT_FLAGS := -load $(POPCORN)/lib/libStackInfo.so -stack-info

# Post-processing
POST_PROCESS_TOOL := $(POPCORN)/bin/gen-stackinfo

SRC	:= $(shell ls *.c)
BC	:= $(SRC:.c=.bc)

LOG := stack-transform.log

###########
# AArch64 #
###########

# Locations
ARM64_POPCORN := $(POPCORN)/aarch64
ARM64_BUILD   := build_aarch64
ARM64_BIN     := $(BIN)_aarch64
ARM64_BC      := $(addprefix $(ARM64_BUILD)/,$(BC))

# Flags
ARM64_TARGET := aarch64-linux-gnu
ARM64_INC    := -isystem $(ARM64_POPCORN)/include
ARM64_LIB    := -L$(ARM64_POPCORN)/lib -L$(ARM64_LIBGCC) \
								$(addprefix $(ARM64_POPCORN),$(LIBS)) -lgcc

##########
# x86-64 #
##########

# Locations
X86_64_POPCORN := $(POPCORN)/x86_64
X86_64_BUILD   := build_x86-64
X86_64_BIN     := $(BIN)_x86-64
X86_64_BC      := $(addprefix $(X86_64_BUILD)/,$(BC))

# Flags
X86_64_TARGET := x86_64-linux-gnu
X86_64_INC    := -isystem $(X86_64_POPCORN)/include
X86_64_LIB    := -L$(X86_64_POPCORN)/lib $(addprefix $(X86_64_POPCORN),$(LIBS))

###############################################################################
#                                 Recipes                                     #
###############################################################################

all: $(ARM64_BIN) $(X86_64_BIN)

clean:
	@echo " [CLEAN] $(ARM64_BIN) $(ARM64_BUILD) $(X86_64_BIN) $(X86_64_BUILD) $(LOG)"
	@rm -rf $(ARM64_BIN) $(ARM64_BUILD) $(X86_64_BIN) $(X86_64_BUILD) $(LOG)

%.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

###########
# AArch64 #
###########

$(ARM64_BUILD)/%.bc: %.c
	@echo " [CC (aarch64)] $<"
	@$(CC) -target $(ARM64_TARGET) $(CFLAGS) $(ARM64_INC) -c -emit-llvm -o $@ $<
	@$(OPT) $(OPT_FLAGS) -o $@ $@

$(ARM64_BIN): $(ARM64_BUILD)/.dir $(ARM64_BC)
	@echo " [CC (aarch64)] $@"
	@$(CC) -target $(ARM64_TARGET) $(CFLAGS) -o $@ $(ARM64_BC) $(ARM64_LIB)
	@$(POST_PROCESS_TOOL) -f $@

##########
# x86-64 #
##########

$(X86_64_BUILD)/%.bc: %.c
	@echo " [CC (x86-64)] $<"
	@$(CC) -target $(X86_64_TARGET) $(CFLAGS) $(X86_64_INC) -c -emit-llvm -o $@ $<
	@$(OPT) $(OPT_FLAGS) -o $@ $@

$(X86_64_BIN): $(X86_64_BUILD)/.dir $(X86_64_BC)
	@echo " [CC (x86-64)] $@"
	@$(CC) -target $(X86_64_TARGET) $(CFLAGS) -o $@ $(X86_64_BC) $(X86_64_LIB)
	@$(POST_PROCESS_TOOL) -f $@

.PHONY: all post_process clean
