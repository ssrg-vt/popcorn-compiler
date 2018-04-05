###############################################################################
#                        System-specific locations                            #
###############################################################################

POPCORN := /usr/local/popcorn

###############################################################################
#                  Compiler toolchain & command-line flags                    #
###############################################################################

# Compiler
CC     := $(POPCORN)/bin/clang
CFLAGS := -O0 -Wall -Wno-unused-variable -g -nostdinc -nostdlib -static \
          -I../../utils -DPOSIX -mllvm -optimize-regalloc
LIBS   := /lib/crt1.o \
          /lib/libc.a \
          /lib/libstack-transform.a \
          /lib/libelf.a \
          /lib/libc.a

OPT       := $(POPCORN)/bin/opt
OPT_FLAGS := -O3 -insert-stackmaps

# Post-processing
POST_PROCESS := $(POPCORN)/bin/gen-stackinfo

SRC	:= $(shell ls *.c)
BC	:= $(SRC:.c=.bc)

LOG := stack-transform.log

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

#$(POWERPC64_BIN)
all: $(ARM64_BIN) $(X86_64_BIN)

clean:
	@echo " [CLEAN] $(ARM64_BIN) $(ARM64_BUILD) $(POWERPC64_BIN) $(POWERPC64_BUILD) $(X86_64_BIN) $(X86_64_BUILD) $(LOG)"
	@rm -rf $(ARM64_BIN) $(ARM64_BUILD) $(POWERPC64_BIN) $(POWERPC64_BUILD) $(X86_64_BIN) $(X86_64_BUILD) $(LOG)

%.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

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
	@$(POST_PROCESS) -f $@

.PHONY: all post_process clean
