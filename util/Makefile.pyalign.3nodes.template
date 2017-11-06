
#                                  FIXME's                                    #
#                                                                             #
#   Fill in these variables with system & application-specific information.   #
#                                                                             #
###############################################################################

# FIXME directory of Popcorn compiler installation
POPCORN := /usr/local/popcorn

# FIXME directory of libgcc & libgcc_eh for aarch64 compiler
ARM64_LIBGCC := $(shell dirname \
                $(shell aarch64-linux-gnu-gcc -print-libgcc-file-name))

# FIXME directory of libgcc & libgcc_eh for powerpc64le compiler
POWERPC64_LIBGCC := $(shell dirname \
                    $(shell powerpc64le-linux-gnu-gcc -print-libgcc-file-name))

#POWERPC64_LIBGCC :=/usr/lib/gcc-cross/powerpc64le-linux-gnu/5.4.0

# FIXME name of the generated executable
BIN := test

# FIXME specify source files
SRC := $(shell ls *.c)

APP_DIR := $(shell pwd)

###############################################################################
#                  Compiler toolchain & command-line flags                    #
###############################################################################

# Compiler
CC         := $(POPCORN)/bin/clang
CXX        := $(POPCORN)/bin/clang++
CFLAGS     := -O0 -Wall -nostdinc -g
HET_CFLAGS := $(CFLAGS) -popcorn-migratable -fno-common \
              -ftls-model=initial-exec

IR := $(SRC:.c=.ll)

# Linker
LD      := $(POPCORN)/bin/ld.gold
LDFLAGS := -z relro --hash-style=gnu --build-id -static
LIBS    := /lib/crt1.o \
           /lib/libc.a \
           /lib/libmigrate.a \
           /lib/libstack-transform.a \
           /lib/libelf.a \
           /lib/libpthread.a \
           /lib/libc.a \
           /lib/libm.a
LIBGCC  := --start-group -lgcc -lgcc_eh --end-group

# Alignment
ALIGN          := $(POPCORN)/bin/pyalign

# Post-processing & checking
POST_PROCESS   := $(POPCORN)/bin/gen-stackinfo
ALIGN_CHECK    := $(POPCORN)/bin/check-align.py
STACKMAP_CHECK := $(POPCORN)/bin/check-stackmaps

###########
# AArch64 #
###########

# Locations
ARM64_POPCORN := $(POPCORN)/aarch64
ARM64_BUILD   := build_aarch64

# Generated files
ARM64_ALIGNED     := $(BIN)_aarch64
ARM64_VANILLA     := $(ARM64_BUILD)/$(ARM64_ALIGNED)
ARM64_OBJ         := $(SRC:.c=.o)
ARM64_MAP         := $(ARM64_BUILD)/map.txt
ARM64_LD_SCRIPT   := $(ARM64_BUILD)/aligned_linker_script_arm.x
ARM64_ALIGNED_MAP := $(ARM64_BUILD)/aligned_map.txt
ARM64_OBJ_TARGET  := $(SRC:.c=_aarch64.o)

# Flags
ARM64_TARGET  := aarch64-linux-gnu
ARM64_INC     := -isystem $(ARM64_POPCORN)/include
ARM64_LDFLAGS := -m aarch64linux -L$(ARM64_POPCORN)/lib -L$(ARM64_LIBGCC) \
                 $(addprefix $(ARM64_POPCORN),$(LIBS)) $(LIBGCC)

#############
# PowerPC64 #
#############

#Locations
POWERPC64_POPCORN := $(POPCORN)/powerpc64
POWERPC64_BUILD   := build_powerpc64

# Generated files
POWERPC64_ALIGNED   := $(BIN)_powerpc64
POWERPC64_VANILLA   := $(POWERPC64_BUILD)/$(POWERPC64_ALIGNED)
POWERPC64_OBJ       := $(SRC:.c=.o)
POWERPC64_MAP         := $(POWERPC64_BUILD)/map.txt
POWERPC64_LD_SCRIPT   := $(POWERPC64_BUILD)/aligned_linker_script_powerpc.x
POWERPC64_ALIGNED_MAP := $(POWERPC64_BUILD)/aligned_map.txt
POWERPC64_OBJ_TARGET  := $(SRC:.c=_powerpc64le.o)

# Flags
POWERPC64_TARGET  := powerpc64le-linux-gnu
POWERPC64_INC     := -isystem $(POWERPC64_POPCORN)/include
POWERPC64_LDFLAGS := -m elf64-powerpcle -L$(POWERPC64_POPCORN)/lib -L$(POWERPC64_LIBGCC) \
 			$(addprefix $(POWERPC64_POPCORN),$(LIBS)) $(LIBGCC)


##########
# x86-64 #
##########

# Locations
X86_64_POPCORN  := $(POPCORN)/x86_64
X86_64_BUILD    := build_x86-64
X86_64_SD_BUILD := sd_x86-64

# Generated files
X86_64_ALIGNED     := $(BIN)_x86-64
X86_64_VANILLA     := $(X86_64_BUILD)/$(X86_64_ALIGNED)
X86_64_OBJ         := $(SRC:.c=.o)
X86_64_MAP         := $(X86_64_BUILD)/map.txt
X86_64_SD          := $(X86_64_SD_BUILD)/$(X86_64_ALIGNED)
X86_64_SD_OBJ      := $(addprefix $(X86_64_SD_BUILD)/,$(SRC:.c=.o))
X86_64_LD_SCRIPT   := $(X86_64_BUILD)/aligned_linker_script_x86.x
X86_64_ALIGNED_MAP := $(X86_64_BUILD)/aligned_map.txt
X86_64_OBJ_TARGET  := $(SRC:.c=_x86_64.o)


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

ir: $(IR)

check: $(ARM64_ALIGNED) $(POWERPC64_ALIGNED) $(X86_64_ALIGNED)
	@echo " [CHECK] Checking alignment for $^"
	@$(ALIGN_CHECK) $(ARM64_ALIGNED) $(X86_64_ALIGNED) $(POWERPC64_ALIGNED)

post_process: post_process_arm64 post_process_ppc64 post_process_x86-64

post_process_arm64: $(ARM64_ALIGNED)
	@echo " [POST_PROCESS] $^"
	@$(POST_PROCESS) -f $(ARM64_ALIGNED)

post_process_ppc64: $(POWERPC64_ALIGNED)
	@echo " [POST_PROCESS] $^"
	@$(POST_PROCESS) -f $(POWERPC64_ALIGNED)

post_process_x86-64: $(X86_64_ALIGNED)
	@echo " [POST_PROCESS] $^"
	@$(POST_PROCESS) -f $(X86_64_ALIGNED)

stack-depth: $(X86_64_SD)

aligned_arm64: $(ARM64_ALIGNED) $(X86_64_ALIGNED)
aligned_ppc64: $(POWERPC64_ALIGNED) $(X86_64_ALIGNED)
aligned-aarch64: $(ARM64_ALIGNED)
aligned-powerpc64: $(POWERPC64_ALIGNED)
aligned-x86-64: $(X86_64_ALIGNED)

vanilla_arm64: $(ARM64_VANILLA) $(X86_64_VANILLA)
vanilla_ppc64: $(POWERPC4_VANILLA) $(X86_64_VANILLA)
vanilla-aarch64: $(ARM64_VANILLA)
vanilla-powerpc64: $(POWERPC64_VANILLA)
vanilla-x86-64: $(X86_64_VANILLA)

clean:
	@echo " [CLEAN] $(POWERPC64_ALIGNED) $(POWERPC64_BUILD) $(X86_64_ALIGNED) \
		$(X86_64_BUILD) $(X86_64_SD_BUILD) \
		$(ARM64_ALIGNED) $(ARM64_BUILD) \
		$(POWERPC64_LD_SCRIPT) $(ARM64_LD_SCRIPT) $(X86_64_LD_SCRIPT) *.ll *.o"

	@rm -rf $(POWERPC64_ALIGNED) $(POWERPC64_BUILD) $(X86_64_ALIGNED)  \
	        $(X86_64_BUILD) $(X86_64_SD_BUILD) \
		$(ARM64_ALIGNED) $(ARM64_BUILD) \
		$(POWERPC64_LD_SCRIPT) $(ARM64_LD_SCRIPT) $(X86_64_LD_SCRIPT) *.ll *.o

%.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

###########
# AArch64 #
###########

$(ARM64_VANILLA): $(ARM64_BUILD)/.dir
	@echo " [CC] $<"
	@$(CC) -target $(ARM64_TARGET) $(HET_CFLAGS) -c $(ARM64_INC) -o $(ARM64_OBJ) $(SRC)
	@echo " [LD] $@ (vanilla)"
	@$(LD) -o $@ $(ARM64_OBJ_TARGET) $(LDFLAGS) $(ARM64_LDFLAGS) -Map $(ARM64_MAP)

$(ARM64_LD_SCRIPT): $(POWERPC64_LD_SCRIPT)
	@echo " [ALIGN] $@"

$(ARM64_ALIGNED): $(ARM64_LD_SCRIPT)
	@echo " [LD] $@ (aligned)"
	@$(LD) -o $@ $(ARM64_OBJ_TARGET) $(LDFLAGS) $(ARM64_LDFLAGS) -Map \
		$(ARM64_ALIGNED_MAP) -T $<

#############
# PowerPC64 #
#############

$(POWERPC64_VANILLA): $(POWERPC64_BUILD)/.dir
	@echo " [CC] $<"
	@$(CC) -target $(POWERPC64_TARGET) $(HET_CFLAGS) -c $(POWERPC64_INC) -o $(POWERPC64_OBJ) $(SRC)
	@echo " [LD] $@ (vanilla)"
	@$(LD) -o $@ $(POWERPC64_OBJ_TARGET) $(LDFLAGS) $(POWERPC64_LDFLAGS) -Map $(POWERPC64_MAP)

$(POWERPC64_LD_SCRIPT): $(POWERPC64_VANILLA) $(X86_64_VANILLA) $(ARM64_VANILLA)
	@echo " [ALIGN] $@"
	@$(ALIGN) --x86-bin $(X86_64_VANILLA) --ppc-bin $(POWERPC64_VANILLA) \
		--arm-bin $(ARM64_VANILLA) --x86-map $(X86_64_MAP) \
		--ppc-map $(POWERPC64_MAP) --arm-map $(ARM64_MAP) \
		--output-x86-ls $(X86_64_LD_SCRIPT) \
		--output-ppc-ls $(POWERPC64_LD_SCRIPT) \
		--output-arm-ls $(ARM64_LD_SCRIPT)

$(POWERPC64_ALIGNED): $(POWERPC64_LD_SCRIPT)
	@echo " [LD] $@ (aligned)"
	@$(LD) -o $@ $(POWERPC64_OBJ_TARGET) $(LDFLAGS) $(POWERPC64_LDFLAGS) -Map \
	$(POWERPC64_ALIGNED_MAP) -T $<

##########
# x86-64 #
##########

$(X86_64_VANILLA): $(X86_64_BUILD)/.dir
	@echo " [CC] $<"
	@$(CC) -target $(X86_64_TARGET) $(HET_CFLAGS) -c $(X86_64_INC) -o $(X86_64_OBJ) $(SRC)
	@echo " [LD] $@ (vanilla)"
	@$(LD) -o $@ $(X86_64_OBJ_TARGET) $(LDFLAGS) $(X86_64_LDFLAGS) -Map $(X86_64_MAP)

#[harubyy] TODO: $(X86_64_LD_SCRIPT): $(ARM64_LD_SCRIPT)
$(X86_64_LD_SCRIPT): $(POWERPC64_LD_SCRIPT)
	@echo " [ALIGN] $@"

$(X86_64_ALIGNED): $(X86_64_LD_SCRIPT)
	@echo " [LD] $@ (aligned)"
	@$(LD) -o $@ $(X86_64_OBJ_TARGET) $(LDFLAGS) $(X86_64_LDFLAGS) \
		-Map $(X86_64_ALIGNED_MAP) -T $<

# Stack-depth builds
$(X86_64_SD_BUILD)/%.o: %.c
	@echo " [CC (x86-64)] $< (stack depth)"
	@$(CC) -target $(X86_64_TARGET) $(CFLAGS) -finstrument-functions -c $(X86_64_INC) -o $@ $<

$(X86_64_SD): $(X86_64_SD_BUILD)/.dir $(X86_64_SD_OBJ)
	@echo " [LD] $@ (stack depth)"
	@$(CXX) -static -L$(POPCORN)/lib -o $@ $(X86_64_SD_OBJ) -lstack-depth

.PHONY: all post_process_ppc64 stack-depth clean \
        aligned_ppc64 aligned-powerpc64 aligned-x86-64 \
        aligned_ppc64 aligned-powerpc64 aligned-x86-64 \
        aligned_arm64 aligned-aarch64 aligned-x86-64 \
        vanilla_arm64 vanilla-aarch64 vanilla-x86-64 \
        vanilla_ppc64 vanilla-powerpc64 vanilla-x86-64 \
		post_process_arm64 post_process_x86-64 post_process

