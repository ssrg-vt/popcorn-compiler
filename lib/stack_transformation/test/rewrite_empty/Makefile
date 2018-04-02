# -------------------
# Compilation options
# -------------------
POPCORN_INSTALL=/usr/local/popcorn/
POPCORN_SRC=/home/pierre/Desktop/hermit-llvm/popcorn-compiler
HERMIT_PREFIX=/home/usr/hermit/

CFLAGS=-target x86_64-hermit -O0 -Wall -Wno-unused-variable -g -nostdlib \
	   -I$(POPCORN_SRC)/lib/stack_transformation/utils -DPOSIX -mllvm \
	   -optimize-regalloc -isystem $(HERMIT_PREFIX)/x86_64-hermit/include
CC=$(POPCORN_INSTALL)/bin/clang
LD=/home/pierre/Desktop/hermit-llvm/hermit-toolchain/build/binutils/gold/ld-new
SRC=rewrite_empty.c
PROG=rewrite_empty_x86-64
BITCODE=$(PROG).bc
TMP_FOLDER=build_x86-64/
LINKER_SCRIPT=ls.x
LIBS=$(HERMIT_PREFIX)/x86_64-hermit/lib/crt0.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/libgcc.a \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crti.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crtbegin.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crtend.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crtn.o \
	 $(HERMIT_PREFIX)/x86_64-hermit/lib/libhermit.a \
	 $(HERMIT_PREFIX)/x86_64-hermit/lib/libm.a \
	 $(HERMIT_PREFIX)/x86_64-hermit/lib/libpthread.a \
	 $(HERMIT_PREFIX)/x86_64-hermit/lib/libc.a \
	 $(POPCORN_SRC)/lib/stack_transformation/build/x86_64/libstack-transform.a \
	 $(HERMIT_PREFIX)/x86_64-hermit/lib/libc.a

# ----------------------
# Hermit runtime options
# ----------------------
PROXY=$(HERMIT_PREFIX)/bin/proxy
ISLE=uhyve
MEM?=1G
VERBOSE?=0
CPUS?=1


all: $(PROG)

.PHONY: $(PROG)
$(PROG):
	@# Create folder if needed
	mkdir -p $(TMP_FOLDER); touch $(TMP_FOLDER)/.dir
	@# 1. emit bitcode
	$(CC) $(CFLAGS) -c $(SRC) -emit-llvm -o $(TMP_FOLDER)/$(BITCODE)
	@# 2. Optimize
	$(POPCORN_INSTALL)/bin/opt -O3 -insert-stackmaps \
		-o $(TMP_FOLDER)/$(BITCODE) $(TMP_FOLDER)/$(BITCODE)
	@# 3. Compile into object file
	$(CC) $(CFLAGS) -c $(TMP_FOLDER)/$(BITCODE) -o $(PROG).o
	@# 4. Link
	$(LD) -T $(LINKER_SCRIPT) -o $(PROG) $(PROG).o $(LIBS)
	@# 5. Post-process
	$(POPCORN_INSTALL)/bin/gen-stackinfo -f $(PROG)	
	@# 6. Change OSABI
	$(HERMIT_PREFIX)/bin/x86_64-hermit-elfedit --output-osabi=HermitCore $(PROG)

test: all
	sudo HERMIT_ISLE=$(ISLE) HERMIT_MEM=$(MEM) HERMIT_CPUS=$(CPUS) \
		HERMIT_VERBOSE=$(VEBOSE) $(PROXY) $(PROG)
