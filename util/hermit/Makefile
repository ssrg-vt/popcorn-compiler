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
CFLAGS+=-O3 -Wall -Wno-unused-variable -g -nostdlib \
	    -DPOSIX -mllvm -optimize-regalloc -mllvm -fast-isel=false -static \
		-popcorn-alignment
CFLAGS_X86=$(CFLAGS) -target x86_64-hermit
CFLAGS_ARM=$(CFLAGS) -target aarch64-hermit -fno-builtin #TODO fnobuiltin needed?

# Include paths (common / x86 only / arm only)
INC=-I$(POPHERMIT)/lib/stack_transformation_hermit/utils \
	-isystem $(PH_PREFIX_HOST)/lib/clang/3.7.1/include
INC_X86=$(INC) -isystem $(PH_PREFIX_X86)/include -nostdinc
INC_ARM=$(INC) -isystem $(PH_PREFIX_ARM)/include -nostdinc

# Compiler and linker
CC=$(PH_PREFIX_HOST)/bin/clang
LD=$(PH_PREFIX_HOST)/bin/x86_64-hermit-ld.gold

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
LIBS=-lhermit -lstack-transform -lelf -lm -lc -lgcc

# Kernel library
KERNEL_X86=$(PH_PREFIX_X86)/lib/libhermit.a
KERNEL_ARM=$(PH_PREFIX_ARM)/lib/libhermit.a

# LDFLAGS (common / x86 only / arm only)
LDFLAGS=-static
LDFLAGS_X86=$(LDFLAGS) -L$(PH_PREFIX_X86)/lib -T $(LINKER_SCRIPT_X86)
LDFLAGS_ARM=$(LDFLAGS) -L$(PH_PREFIX_ARM)/lib -T $(LINKER_SCRIPT_ARM)

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

all: $(PROG_X86) $(PROG_ARM)

%.x86.o: %.c
	@# 1. emit bitcode
	$(CC) $(CFLAGS_X86) $(INC_X86) -c $< -emit-llvm -o $<.x86.bc
	@# 2. Optimize
	$(PH_PREFIX_HOST)/bin/opt -O3 -optimize-regalloc -insert-stackmaps \
		-o $<.x86.bc $<.x86.bc
	@# 3. Compile into object file
	$(CC) $(CFLAGS_X86)	-c $<.x86.bc -o $@

%.arm.o: %.c
	@# 1. emit bitcode
	$(CC) $(CFLAGS_ARM) $(INC_ARM) -c $< -emit-llvm -o $<.arm.bc
	@# 2. Optimize
	$(PH_PREFIX_HOST)/bin/opt -O3 -optimize-regalloc -insert-stackmaps \
		-o $<.arm.bc $<.arm.bc
	@# 3. Compile into object file
	$(CC) $(CFLAGS_ARM)	-c $<.arm.bc -o $@

$(PROG_X86): $(OBJS_X86) $(KERNEL_X86)
	@# 4. Link
	$(LD) -o $(PROG_X86) $(OBJS_X86) $(ADD_OBJS_X86) $(LIBS) $(LDFLAGS_X86)
	@# 5. Post-process
	$(PH_PREFIX_HOST)/bin/gen-stackinfo -f $(PROG_X86)
	@# 6. Change OSABI
	$(PH_PREFIX_HOST)/bin/x86_64-hermit-elfedit --output-osabi=HermitCore \
		$(PROG_X86)

$(PROG_ARM): $(OBJS_ARM) $(KERNEL_ARM)
	@# 4. Link
	$(LD) -o $(PROG_ARM) $(OBJS_ARM) $(ADD_OBJS_ARM) $(LIBS) $(LDFLAGS_ARM)
	@# 5. Post-process
	$(PH_PREFIX_HOST)/bin/gen-stackinfo -f $(PROG_ARM)
	@# 6. Change OSABI
	$(PH_PREFIX_HOST)/bin/x86_64-hermit-elfedit --output-osabi=HermitCore \
		$(PROG_ARM)

test-x86: $(PROG_X86)
	HERMIT_ISLE=uhyve HERMIT_MEM=$(MEM) HERMIT_CPUS=$(CPUS) \
		HERMIT_VERBOSE=$(VERBOSE) HERMIT_MIGTEST=$(MIGTEST) \
		HERMIT_MIGRATE_RESUME=$(RESUME) HERMIT_DEBUG=$(DEBUG) \
		$(PROXY_X86) $(PROG_X86) $(ARGS)

test-arm: $(PROG_ARM)
	scp $(PROXY_ARM) potato:/tmp/proxy
	scp $(PROG_ARM) potato:/tmp
	ssh potato HERMIT_ISLE=uhyve /tmp/proxy /tmp/$(PROG_ARM)

objdump-x86:
	objdump --source $(PROG_X86) > /tmp/objdump-x86.txt && \
		vim /tmp/objdump-x86.txt

objdump-arm:
	objdump --source $(PROG_ARM) > /tmp/objdump-arm.txt && \
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
	rm -rf *.o *.bc $(PROG_X86) $(PROG_ARM) stack-transform.log bss.bin \
	data.bin fds.bin heap.bin mdata.bin stack.bin.* tls.bin.*
