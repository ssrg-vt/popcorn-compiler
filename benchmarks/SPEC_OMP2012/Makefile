POPCORN       := /usr/local/popcorn
COMMON        := $(shell readlink -f $(APP_DIR)/../common)
ARM64_LIBGCC  := $(shell dirname \
                 $(shell aarch64-linux-gnu-gcc -print-libgcc-file-name))
X86_64_LIBGCC := $(shell dirname \
                 $(shell x86_64-linux-gnu-gcc -print-libgcc-file-name))
 
###############################################################################
#                  Compiler toolchain & command-line flags                    #
###############################################################################

# Default optimization level if application Makefile did not specify
OPT ?= -O2

# Compiler
CC           := $(POPCORN)/bin/clang
CXX          := $(POPCORN)/bin/clang++
CFLAGS       += $(OPT) -Wall -g -fopenmp=libiomp5 -fopenmp-use-tls -nostdinc \
                -I$(COMMON)
CXXFLAGS     += $(CFLAGS) -nostdinc++	
ifneq ($(findstring optimized,$(type)),)
CFLAGS       += -D_ALIGN_LAYOUT -D_OPTIMIZED -distributed-omp
CXXFLAGS     += -D_ALIGN_LAYOUT -D_OPTIMIZED -distributed-omp
endif
HET_FLAGS		 := -popcorn-metadata -mllvm -no-sm-warn -fno-common \
                -ftls-model=initial-exec
HET_CFLAGS   := $(CFLAGS) $(HET_FLAGS)
HET_CXXFLAGS := $(CXXFLAGS) $(HET_FLAGS)

# Linker
LD      := $(POPCORN)/bin/x86_64-popcorn-linux-gnu-ld.gold
LDFLAGS := -z noexecstack -z relro --hash-style=gnu --build-id -static
LIBS    := /lib/crt1.o \
           /lib/libc.a \
           /lib/libopenpop.a \
           /lib/libmigrate.a \
           /lib/libstack-transform.a \
           /lib/libelf.a \
           /lib/libpthread.a \
           /lib/libc.a
LIBGCC  := --start-group -lgcc -lgcc_eh --end-group

# Alignment
ALIGN          := $(POPCORN)/bin/pyalign

# Post-processing & checking
POST_PROCESS   := $(POPCORN)/bin/gen-stackinfo
COMPRESS       := $(POPCORN)/bin/compress
ALIGN_CHECK    := $(POPCORN)/bin/check-align.py
STACKMAP_CHECK := $(POPCORN)/bin/check-stackmaps

################
# Common files #
################

IR := $(C_SRC:.c=.ll) $(CXX_SRC:.cpp=.ll)

###########
# AArch64 #
###########

# Locations
ARM64_POPCORN := $(POPCORN)/aarch64
ARM64_BUILD   := build_aarch64

# Generated files
ARM64_ALIGNED     := $(BIN)_aarch64
ARM64_VANILLA     := $(ARM64_BUILD)/$(ARM64_ALIGNED)
ARM64_OBJ         := $(C_SRC:.c=_aarch64.o) $(CXX_SRC:.cpp=_aarch64.o)
ARM64_MAP         := $(ARM64_BUILD)/map.txt
ARM64_LD_SCRIPT   := $(ARM64_BUILD)/aligned_linker_script_arm.x
ARM64_ALIGNED_MAP := $(ARM64_BUILD)/aligned_map.txt

# Flags
ARM64_TARGET  := aarch64-linux-gnu
ARM64_INC     := -isystem $(ARM64_POPCORN)/include
ARM64_LDFLAGS := -m aarch64linux -L$(ARM64_POPCORN)/lib -L$(ARM64_LIBGCC) \
                 $(addprefix $(ARM64_POPCORN),$(LIBS)) $(LIBGCC)

##########
# x86-64 #
##########

# Locations
X86_64_POPCORN  := $(POPCORN)/x86_64
X86_64_BUILD    := build_x86-64

# Generated files
X86_64_ALIGNED     := $(BIN)_x86-64
X86_64_VANILLA     := $(X86_64_BUILD)/$(X86_64_ALIGNED)
X86_64_OBJ         := $(C_SRC:.c=_x86_64.o) $(CXX_SRC:.cpp=_x86_64.o)
X86_64_MAP         := $(X86_64_BUILD)/map.txt
X86_64_LD_SCRIPT   := $(X86_64_BUILD)/aligned_linker_script_x86.x
X86_64_ALIGNED_MAP := $(X86_64_BUILD)/aligned_map.txt

# Flags
X86_64_TARGET  := x86_64-linux-gnu
X86_64_INC     := -isystem $(X86_64_POPCORN)/include
X86_64_LDFLAGS := -m elf_x86_64 -L$(X86_64_POPCORN)/lib \
                  $(addprefix $(X86_64_POPCORN),$(LIBS)) \
                  --start-group --end-group

###############################################################################
#                                 Recipes                                     #
###############################################################################

all: post_process

ir: $(DEPS) $(IR)

check: $(ARM64_ALIGNED) $(X86_64_ALIGNED)
	@echo " [CHECK] Checking stackmaps for $^"
	@$(STACKMAP_CHECK) -a $(ARM64_ALIGNED) -x $(X86_64_ALIGNED)
	@echo " [CHECK] Checking alignment for $^"
	@$(ALIGN_CHECK) $(ARM64_ALIGNED) $(X86_64_ALIGNED)

post_process: $(ARM64_ALIGNED) $(X86_64_ALIGNED)
	@echo " [POST_PROCESS] $^"
	@$(POST_PROCESS) -f $(ARM64_ALIGNED)
	@$(POST_PROCESS) -f $(X86_64_ALIGNED)

compress: $(ARM64_ALIGNED) $(X86_64_ALIGNED)
	@echo " [COMPRESS] $(ARM64_ALIGNED)"
	@$(COMPRESS) -f $(ARM64_ALIGNED)
	@echo " [COMPRESS] $(X86_64_ALIGNED)"
	@$(COMPRESS) -f $(X86_64_ALIGNED)

aligned: $(ARM64_ALIGNED) $(X86_64_ALIGNED)
aligned-aarch64: $(ARM64_ALIGNED)
aligned-x86-64: $(X86_64_ALIGNED)

vanilla: $(ARM64_VANILLA) $(X86_64_VANILLA)
vanilla-aarch64: $(ARM64_VANILLA)
vanilla-x86-64: $(X86_64_VANILLA)

clean:
	@echo " [CLEAN] $(ARM64_ALIGNED) $(ARM64_BUILD) $(X86_64_ALIGNED)" \
		"$(X86_64_BUILD) $(X86_64_LD_SCRIPT) $(ARM64_LD_SCRIPT) $(TRASH) *.ll *.o"
	@rm -rf $(ARM64_ALIGNED) $(ARM64_BUILD) $(X86_64_ALIGNED) $(X86_64_BUILD) \
		$(X86_64_LD_SCRIPT) $(ARM64_LD_SCRIPT) $(TRASH) *.ll *.o

%.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

##########
# Common #
##########

%.ll: %.c
	@echo " [IR] $<"
	@$(CC) $(HET_CFLAGS) -S -emit-llvm $(ARM64_INC) -o $@ $<

%.ll: %.cpp
	@echo " [IR] $<"
	@$(CXX) $(HET_CXXFLAGS) -S -emit-llvm $(ARM64_INC) -o $@ $<

###########
# AArch64 #
###########

%_aarch64.o: %.c
	@echo " [CC] $<"
	@$(CC) $(HET_CFLAGS) -c $(ARM64_INC) -o $(<:.c=.o) $<

%_aarch64.o: %.cpp
	@echo " [CXX] $<"
	@$(CXX) $(HET_CXXFLAGS) -c $(ARM64_INC) -o $(<:.cpp=.o) $<

$(ARM64_VANILLA): $(DEPS) $(ARM64_BUILD)/.dir $(ARM64_OBJ)
	@echo " [LD] $@ (vanilla)"
	@$(LD) -o $@ $(ARM64_OBJ) $(LDFLAGS) $(ARM64_LDFLAGS) -Map $(ARM64_MAP)

$(ARM64_LD_SCRIPT): $(ARM64_VANILLA) $(X86_64_VANILLA) 
	@echo " [ALIGN] $@"
	@$(ALIGN) --compiler-inst $(POPCORN) \
		--x86-bin $(X86_64_VANILLA) --arm-bin $(ARM64_VANILLA) \
		--x86-map $(X86_64_MAP) --arm-map $(ARM64_MAP) \
		--output-x86-ls $(X86_64_LD_SCRIPT)	--output-arm-ls $(ARM64_LD_SCRIPT)

$(ARM64_ALIGNED): $(ARM64_LD_SCRIPT)
	@echo " [LD] $@ (aligned)"
	@$(LD) -o $@ $(ARM64_OBJ) $(LDFLAGS) $(ARM64_LDFLAGS) -Map \
		$(ARM64_ALIGNED_MAP) -T $<

##########
# x86-64 #
##########

%_x86_64.o: %_aarch64.o

$(X86_64_VANILLA): $(DEPS) $(X86_64_BUILD)/.dir $(X86_64_OBJ)
	@echo " [LD] $@ (vanilla)"
	@$(LD) -o $@ $(X86_64_OBJ) $(LDFLAGS) $(X86_64_LDFLAGS) -Map $(X86_64_MAP)

$(X86_64_LD_SCRIPT): $(ARM64_LD_SCRIPT)
	@echo " [ALIGN] $@"

$(X86_64_ALIGNED): $(X86_64_LD_SCRIPT)
	@echo " [LD] $@ (aligned)"
	@$(LD) -o $@ $(X86_64_OBJ) $(LDFLAGS) $(X86_64_LDFLAGS) \
		-Map $(X86_64_ALIGNED_MAP) -T $<

.PHONY: all post_process clean \
        aligned aligned-aarch64 aligned-x86-64 \
        vanilla vanilla-aarch64 vanilla-x86-64
