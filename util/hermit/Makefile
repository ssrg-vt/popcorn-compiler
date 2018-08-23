# -------------------
# Compilation options
# -------------------

# Path to hermit-popcorn installation
POPHERMIT=$(HOME)/hermit-popcorn
POPHERMIT_PREFIX=$(POPHERMIT)

PH_PREFIX_HOST=$(POPHERMIT_PREFIX)/x86_64-host/
PH_PREFIX_X86=$(POPHERMIT_PREFIX)/x86_64-hermit/
PH_PREFIX_ARM=$(POPHERMIT_PREFIX)/aarch64-hermit/

# CFLAGS (common / x86 only / arm only)
CFLAGS+=-O1 -Wall -Wno-unused-variable -g -nostdlib -D_GNU_SOURCE \
	    -DPOSIX -mllvm -optimize-regalloc -mllvm -fast-isel=false -static \
		-popcorn-alignment -ffunction-sections -fdata-sections -fno-common
CFLAGS_X86=$(CFLAGS) -target x86_64-hermit
CFLAGS_ARM=$(CFLAGS) -target aarch64-hermit

# Include paths (common / x86 only / arm only)
INC=-I$(POPHERMIT)/lib/stack_transformation_hermit/utils \
	-isystem $(PH_PREFIX_HOST)/lib/clang/3.7.1/include
INC_X86=$(INC) -isystem $(PH_PREFIX_X86)/include -nostdinc
INC_ARM=$(INC) -isystem $(PH_PREFIX_ARM)/include -nostdinc

# Compiler and linker
CC=$(PH_PREFIX_HOST)/bin/clang
LD=$(PH_PREFIX_HOST)/bin/x86_64-hermit-ld.gold

# Options to pass to LLVM opt
OPT_OPTIONS=-O1 -optimize-regalloc -insert-stackmaps

SRCS=$(shell ls *.c)
OBJS_X86=$(SRCS:.c=.x86.o)
OBJS_ARM=$(SRCS:.c=.arm.o)

PROG_X86=prog_x86-64
PROG_ARM=prog_aarch64
LINKER_SCRIPT_X86=$(PH_PREFIX_X86)/lib/ls.x
LINKER_SCRIPT_ARM=$(PH_PREFIX_ARM)/lib/ls.x

# Additional object files to link with any application (x86)
ADD_OBJS_X86=$(PH_PREFIX_X86)/lib/crt0.o \
		 $(PH_PREFIX_X86)/lib/crti.o \
		 $(PH_PREFIX_X86)/lib/crtbegin.o \
		 $(PH_PREFIX_X86)/lib/crtend.o \
		 $(PH_PREFIX_X86)/lib/crtn.o

# Additional object files to link with any application (arm)
ADD_OBJS_ARM=$(PH_PREFIX_ARM)/lib/crt0.o \
		 $(PH_PREFIX_ARM)/lib/crti.o \
		 $(PH_PREFIX_ARM)/lib/crtbegin.o \
		 $(PH_PREFIX_ARM)/lib/crtend.o \
		 $(PH_PREFIX_ARM)/lib/crtn.o

# Libraries inclusions (for both x86 and arm)
LIBS=-lmigrate -lstack-transform -lhermit -lelf -lm -lc -lgcc

# Kernel library
KERNEL_X86=$(PH_PREFIX_X86)/lib/libhermit.a
KERNEL_ARM=$(PH_PREFIX_ARM)/lib/libhermit.a

# Deps: libmigrate and libstack_transform
LMIGRATE_X86=$(PH_PREFIX_X86)/lib/libmigrate.a
LSTACKTRASNFORM_X86=$(PH_PREFIX_X86)/lib/libstack-transform.a
LMIGRATE_ARM=$(PH_PREFIX_ARM)/lib/libmigrate.a
LSTACKTRASNFORM_ARM=$(PH_PREFIX_ARM)/lib/libstack-transform.a

# Generated unaligned MAP files
MAP_X86=map_x86-64.txt
MAP_ARM=map_aarch64.txt

# Generated aligned MAP files
MAP_ALIGNED_X86=aligned_map_x86-64.txt
MAP_ALIGNED_ARM=aligned_map_aarch64.txt


# Generated Linker Scripts
LINKER_SCRIPT_ALIGNED_X86=aligned_linker_script_x86.x
LINKER_SCRIPT_ALIGNED_ARM=aligned_linker_script_aarch64.x

#Final Aligned Executable
PROG_X86_ALIGNED=$(PROG_X86)_aligned
PROG_ARM_ALIGNED=$(PROG_ARM)_aligned

# alignment
ALIGN=$(PH_PREFIX_HOST)/bin/pyalign
ALIGN_CHECK=$(PH_PREFIX_HOST)/bin/check-align.py
# LDFLAGS (common / x86 only / arm only)
LDFLAGS=-static
LDFLAGS_X86=$(LDFLAGS) -L$(PH_PREFIX_X86)/lib -T $(LINKER_SCRIPT_X86) -Map $(MAP_X86)
LDFLAGS_ARM=$(LDFLAGS) -L$(PH_PREFIX_ARM)/lib -T $(LINKER_SCRIPT_ARM) -Map $(MAP_ARM)
LDFLAGS_ALIGN_X86=$(LDFLAGS) -L$(PH_PREFIX_X86)/lib -T $(LINKER_SCRIPT_ALIGNED_X86) -Map $(MAP_ALIGNED_X86)
LDFLAGS_ALIGN_ARM=$(LDFLAGS) -L$(PH_PREFIX_ARM)/lib -T $(LINKER_SCRIPT_ALIGNED_ARM) -Map $(MAP_ALIGNED_ARM)

# ----------------------
# Hermit runtime options
# ----------------------
PROXY_X86=$(PH_PREFIX_HOST)/bin/proxy
PROXY_ARM=$(PH_PREFIX_ARM)/bin/proxy
MEM?=2G
VERBOSE?=0
CPUS?=1
ARGS?=
MIGTEST?=0
RESUME?=0
DEBUG?=0
ARM_TARGET_IP?=potato
ARM_TARGET_HOME := $(shell ssh $(ARM_TARGET_IP) "pwd")

# Transfer method can be 'rsync --no-whole-file' or ssh, rsync is faster
TRANFER_METHOD=rsync --no-whole-file

all: $(PROG_X86_ALIGNED) $(PROG_ARM_ALIGNED)

check: all
	$(ALIGN_CHECK) $(PROG_X86_ALIGNED) $(PROG_ARM_ALIGNED)

%.x86.o: %.c
	@echo "[x86_64-cc] $<.x86.o"
	@# 1. emit bitcode
	@$(CC) $(CFLAGS_X86) $(INC_X86) -c $< -emit-llvm -o $<.x86.bc
	@# 2. Optimize
	@$(PH_PREFIX_HOST)/bin/opt $(OPT_OPTIONS) -o $<.x86.bc $<.x86.bc
	@# 3. Compile into object file
	@$(CC) $(CFLAGS_X86)	-c $<.x86.bc -o $@

%.arm.o: %.c
	@echo "[aarch64-cc] $<.x86.o"
	@# 1. emit bitcode
	@$(CC) $(CFLAGS_ARM) $(INC_ARM) -c $< -emit-llvm -o $<.arm.bc
	@# 2. Optimize
	@$(PH_PREFIX_HOST)/bin/opt $(OPT_OPTIONS) -o $<.arm.bc $<.arm.bc
	@# 3. Compile into object file
	@$(CC) $(CFLAGS_ARM)	-c $<.arm.bc -o $@

$(PROG_X86): $(OBJS_X86) $(KERNEL_X86) $(LMIGRATE_X86) $(LSTACKTRASNFORM_X86)
	@echo "[ld-unaligned] $(PROG_X86)"
	@# 4. Link
	@$(LD) -o $(PROG_X86) $(OBJS_X86) $(ADD_OBJS_X86) $(LIBS) $(LDFLAGS_X86)

$(PROG_ARM): $(OBJS_ARM) $(KERNEL_ARM) $(LMIGRATE_ARM) $(LSTACKTRASNFORM_ARM)
	@echo "[ld-unaligned] $(PROG_ARM)"
	@# 4. Link
	@$(LD) -o $(PROG_ARM) $(OBJS_ARM) $(ADD_OBJS_ARM) $(LIBS) $(LDFLAGS_ARM)

$(LINKER_SCRIPT_ALIGNED_X86) $(LINKER_SCRIPT_ALIGNED_ARM): $(PROG_X86) $(PROG_ARM)
	@echo "[align] $(PROG_X86) $(PROG_ARM)"
	@#5. Align
	@$(ALIGN) --compiler-inst $(PH_PREFIX_HOST) \
	--x86-bin $(PROG_X86) --arm-bin $(PROG_ARM) \
	--x86-map $(MAP_X86) --arm-map $(MAP_ARM) \
	--output-x86-ls $(LINKER_SCRIPT_ALIGNED_X86) \
	--output-arm-ls $(LINKER_SCRIPT_ALIGNED_ARM)

$(PROG_X86_ALIGNED): $(LINKER_SCRIPT_ALIGNED_X86) $(KERNEL_X86)
	@echo "[ld-aligned] $(PROG_X86_ALIGNED)"
	@# 6. Link with aligned script
	@$(LD) -o $(PROG_X86_ALIGNED) $(OBJS_X86) $(ADD_OBJS_X86) $(LIBS) $(LDFLAGS_ALIGN_X86)
	@# 7. Post-process
	@$(PH_PREFIX_HOST)/bin/gen-stackinfo -f $(PROG_X86_ALIGNED)
	@# 8. Change OSABI
	@$(PH_PREFIX_HOST)/bin/x86_64-hermit-elfedit --output-osabi=HermitCore \
		$(PROG_X86_ALIGNED)

$(PROG_ARM_ALIGNED): $(LINKER_SCRIPT_ALIGNED_ARM) $(KERNEL_ARM)
	@echo "[ld-aligned] $(PROG_ARM_ALIGNED)"
	@# 6. Link with aligned script
	@$(LD) -o $(PROG_ARM_ALIGNED) $(OBJS_ARM) $(ADD_OBJS_ARM) $(LIBS) $(LDFLAGS_ALIGN_ARM)
	@# 7. Post-process
	@$(PH_PREFIX_HOST)/bin/gen-stackinfo -f $(PROG_ARM_ALIGNED)
	@# 8. Change OSABI
	@$(PH_PREFIX_HOST)/bin/x86_64-hermit-elfedit --output-osabi=HermitCore \
		$(PROG_ARM_ALIGNED)

test-x86: $(PROG_X86_ALIGNED) $(PROG_ARM_ALIGNED)
	HERMIT_ISLE=uhyve HERMIT_MEM=$(MEM) HERMIT_CPUS=$(CPUS) \
		HERMIT_VERBOSE=$(VERBOSE) HERMIT_MIGTEST=$(MIGTEST) \
		HERMIT_MIGRATE_RESUME=$(RESUME) HERMIT_DEBUG=$(DEBUG) \
		HERMIT_NODE_ID=0 ST_AARCH64_BIN=$(PROG_ARM_ALIGNED) \
		ST_X86_64_BIN=$(PROG_X86_ALIGNED) \
		$(PROXY_X86) $(PROG_X86_ALIGNED) $(ARGS)

test-arm: $(PROG_ARM_ALIGNED) $(PROG_X86_ALIGNED)
	@ssh $(ARM_TARGET_IP) "rm -rf $(ARM_TARGET_HOME)/proxy && killall -q -9 proxy; true"
	$(TRANFER_METHOD) $(PROXY_ARM) $(PROG_ARM_ALIGNED) $(PROG_X86_ALIGNED) $(ARM_TARGET_IP):$(ARM_TARGET_HOME)/
	ssh $(ARM_TARGET_IP) HERMIT_ISLE=uhyve HERMIT_MEM=$(MEM) HERMIT_CPUS=$(CPUS) \
		HERMIT_VERBOSE=$(VERBOSE) HERMIT_MIGTEST=$(MIGTEST) \
		HERMIT_MIGRATE_RESUME=$(RESUME) HERMIT_DEBUG=$(DEBUG) \
		HERMIT_NODE_ID=1 ST_AARCH64_BIN=$(PROG_ARM_ALIGNED) \
		ST_X86_64_BIN=$(PROG_X86_ALIGNED) \
		$(ARM_TARGET_HOME)/proxy $(ARM_TARGET_HOME)/$(PROG_ARM_ALIGNED) $(ARGS)

transfer-checkpoint-to-arm:
	$(TRANFER_METHOD) *.bin stack.bin.* tls.bin.* $(ARM_TARGET_IP):$(ARM_TARGET_HOME)

transfer-checkpoint-from-arm:
	$(TRANFER_METHOD) $(ARM_TARGET_IP):$(ARM_TARGET_HOME)/*.bin .
	$(TRANFER_METHOD) $(ARM_TARGET_IP):$(ARM_TARGET_HOME)/stack.bin.* .
	$(TRANFER_METHOD) $(ARM_TARGET_IP):$(ARM_TARGET_HOME)/tls.bin.* .

objdump-x86:
	objdump --source $(PROG_X86_ALIGNED) > /tmp/objdump-x86.txt && \
		vim /tmp/objdump-x86.txt

objdump-arm:
	aarch64-linux-gnu-objdump --source $(PROG_ARM_ALIGNED) > /tmp/objdump-arm.txt && \
		vim /tmp/objdump-arm.txt

dump-stackinfo-x86:
	$(PH_PREFIX_HOST)/bin/dump-stackinfo -b $(PROG_X86)

dump-stackinfo-arm:
	$(PH_PREFIX_HOST)/bin/dump-stackinfo -b $(PROG_ARM)

dump-llvm-stackmaps-x86:
	$(PH_PREFIX_HOST)/bin/dump-llvm-stackmap -f $(PROG_X86)

dump-llvm-stackmaps-arm:
	$(PH_PREFIX_HOST)/bin/dump-llvm-stackmap -f $(PROG_ARM)

clean:
	rm -rf *.o *.bc $(PROG_X86) $(PROG_ARM) \
	$(PROG_X86_ALIGNED) $(PROG_ARM_ALIGNED) \
	stack-transform.log bss.bin map* aligned* \
	data.bin fds.bin heap.bin mdata.bin stack.bin.* tls.bin.*
