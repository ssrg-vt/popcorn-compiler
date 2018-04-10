# -------------------
# Compilation options
# -------------------

POPHERMIT=/home/pierre/Desktop/hermit-llvm/popcorn-compiler/
POPHERMIT_PREFIX=$(POPHERMIT)/prefix/

PH_PREFIX_HOST=$(POPHERMIT_PREFIX)/x86_64-host/
PH_PREFIX_X86=$(POPHERMIT_PREFIX)/x86_64-hermit/

# TODO delete this when I can
HERMIT_PREFIX=/home/usr/hermit/

CFLAGS=-target x86_64-hermit -O0 -Wall -Wno-unused-variable -g -nostdlib \
	   -I$(POPHERMIT)/lib/stack_transformation_hermit/utils -DPOSIX -mllvm \
	   -optimize-regalloc -isystem $(PH_PREFIX_X86)/include \
	   -isystem $(HERMIT_PREFIX)/x86_64-hermit/include -nostdinc \
	   -isystem $(PH_PREFIX_HOST)/lib/clang/3.7.1/include
CC=$(PH_PREFIX_HOST)/bin/clang
LD=$(PH_PREFIX_HOST)/bin/x86_64-hermit-ld.gold
SRC=$(shell ls *.c)
PROG=prog_x86-64
BITCODE=$(PROG).bc
TMP_FOLDER=build_x86-64/
LINKER_SCRIPT=$(POPHERMIT)/util/hermit/ls.x
LIBS=$(PH_PREFIX_X86)/lib/crt0.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/libgcc.a \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crti.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crtbegin.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crtend.o \
	 $(HERMIT_PREFIX)/lib/gcc/x86_64-hermit/6.3.0/crtn.o \
	 $(PH_PREFIX_X86)/lib/libpthread.a \
	 $(PH_PREFIX_X86)/lib/libhermit.a \
	 $(PH_PREFIX_X86)/lib/libstack-transform.a \
	 $(PH_PREFIX_X86)/lib/libelf.a \
	 $(PH_PREFIX_X86)/lib/libm.a  \
	 $(PH_PREFIX_X86)/lib/libc.a

# ----------------------
# Hermit runtime options
# ----------------------
PROXY=$(PH_PREFIX_HOST)/bin/proxy
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
	$(PH_PREFIX_HOST)/bin/opt -O3 -insert-stackmaps \
		-o $(TMP_FOLDER)/$(BITCODE) $(TMP_FOLDER)/$(BITCODE)
	@# 3. Compile into object file
	$(CC) $(CFLAGS) -c $(TMP_FOLDER)/$(BITCODE) -o $(PROG).o
	@# 4. Link
	$(LD) -T $(LINKER_SCRIPT) -o $(PROG) $(PROG).o $(LIBS)
	@# 5. Post-process
	$(PH_PREFIX_HOST)/bin/gen-stackinfo -f $(PROG)
	@# 6. Change OSABI
	$(PH_PREFIX_HOST)/bin/x86_64-hermit-elfedit --output-osabi=HermitCore $(PROG)

test: all
	HERMIT_ISLE=$(ISLE) HERMIT_MEM=$(MEM) HERMIT_CPUS=$(CPUS) \
		HERMIT_VERBOSE=$(VERBOSE) $(PROXY) $(PROG)

clean:
	rm -rf $(TMP_FOLDER) *.o $(PROG)
