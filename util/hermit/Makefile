# -------------------
# Compilation options
# -------------------

POPHERMIT=$(HOME)/hermit-popcorn
POPHERMIT_PREFIX=$(POPHERMIT)

PH_PREFIX_HOST=$(POPHERMIT_PREFIX)/x86_64-host/
PH_PREFIX_X86=$(POPHERMIT_PREFIX)/x86_64-hermit/

CFLAGS+=-target x86_64-hermit -O0 -Wall -Wno-unused-variable -g -nostdlib \
	    -DPOSIX -mllvm -optimize-regalloc -mllvm -fast-isel=false -static \
		-popcorn-alignment

INC=-I$(POPHERMIT)/lib/stack_transformation_hermit/utils \
	-isystem $(PH_PREFIX_HOST)/lib/clang/3.7.1/include \
	-isystem $(PH_PREFIX_X86)/include -nostdinc

CC=$(PH_PREFIX_HOST)/bin/clang
LD=$(PH_PREFIX_HOST)/bin/x86_64-hermit-ld.gold

SRCS=$(shell ls *.c)
OBJS=$(SRCS:.c=.o)

PROG=prog_x86-64
TMP_FOLDER=build_x86-64/
LINKER_SCRIPT=$(PH_PREFIX_X86)/lib/ls.x
ADD_OBJS=$(PH_PREFIX_X86)/lib/crt0.o \
		 $(PH_PREFIX_X86)/lib/crti.o \
		 $(PH_PREFIX_X86)/lib/crtbegin.o \
		 $(PH_PREFIX_X86)/lib/crtend.o \
		 $(PH_PREFIX_X86)/lib/crtn.o

LIBS=-lpthread -lhermit -lstack-transform -lelf -lm -lc

LDFLAGS=-L$(PH_PREFIX_X86)/lib

# ----------------------
# Hermit runtime options
# ----------------------
PROXY=$(PH_PREFIX_HOST)/bin/proxy
ISLE=uhyve
MEM?=4G
VERBOSE?=0
CPUS?=1

all: $(PROG)

%.o: %.c
	@#0. Create tmp folder if needed
	mkdir -p $(TMP_FOLDER) && touch $(TMP_FOLDER)/.dir
	@# 1. emit bitcode
	$(CC) $(CFLAGS) $(INC) -c $< -emit-llvm -o $(TMP_FOLDER)/$<.bc
	@# 2. Optimize
	$(PH_PREFIX_HOST)/bin/opt -O3 -insert-stackmaps \
		-o $(TMP_FOLDER)/$<.bc $(TMP_FOLDER)/$<.bc
	@# 3. Compile into object file
	$(CC) -target x86_64-hermit -static -mllvm -optimize-regalloc \
		-c $(TMP_FOLDER)/$<.bc -o $@

$(PROG): $(OBJS)
	@# 4. Link
	$(LD) -T $(LINKER_SCRIPT) -o $(PROG) $(OBJS) $(ADD_OBJS) $(LIBS) $(LDFLAGS)
	@# 5. Post-process
	$(PH_PREFIX_HOST)/bin/gen-stackinfo -f $(PROG)
	@# 6. Change OSABI
	$(PH_PREFIX_HOST)/bin/x86_64-hermit-elfedit --output-osabi=HermitCore $(PROG)

test: all
	HERMIT_ISLE=$(ISLE) HERMIT_MEM=$(MEM) HERMIT_CPUS=$(CPUS) \
		HERMIT_VERBOSE=$(VERBOSE) $(PROXY) $(PROG)

objdump:
	objdump --source $(PROG) > /tmp/objdump.txt && vim /tmp/objdump.txt

dump-stackinfo:
	$(PH_PREFIX_HOST)/bin/dump-stackinfo -b $(PROG)

dump-llvm-stackmaps:
	$(PH_PREFIX_HOST)/bin/dump-llvm-stackmap -f $(PROG)

clean:
rm -rf $(TMP_FOLDER) *.o $(PROG) stack-transform.log
