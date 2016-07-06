##########################################################################
# Copyright (c) 2012, 2013, 2014, 2015, SSRG Virginia Tech
# Copyright (c) 2007, 2008, 2009, ETH Zurich.
# All rights reserved.
#
# This file is distributed under the terms in the attached LICENSE file.
# If you do not find this file, copies can be found by writing to:
# ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
##########################################################################

POPCORN = /usr/local/popcorn

CC     = clang
CFLAGS = -O3 -g -gdwarf-aranges -DPOSIX -fno-stack-protector \
         -ffunction-sections -fdata-sections

OPT       = opt
OPT_FLAGS = -load $(POPCORN)/lib/libStackInfo.so -stack-info

SRC = kmp.c linux_backend.c omp.c parallel.c processing.c self_info.c
OBJ = $(SRC:.c=.o)

# aarch64
CFLAGS_AARCH64 = $(CFLAGS) -target aarch64-linux-gnu -isystem \
                 $(POPCORN)/aarch64/include -D__AARCH64
BUILD_AARCH64  = build_aarch64
OBJ_AARCH64    = $(addprefix $(BUILD_AARCH64)/,$(OBJ))

# x86-64
CFLAGS_X86_64 = $(CFLAGS) -target x86_64-linux-gnu -isystem \
                $(POPCORN)/x86_64/include -m64 -D__X86
BUILD_X86_64  = build_x86_64
OBJ_X86_64    = $(addprefix $(BUILD_X86_64)/,$(OBJ))

all: libbomp_aarch64.a libbomp_x86-64.a

%/.dir:
	@mkdir -p $*
	@touch $@

$(BUILD_AARCH64)/%.o: %.c
	$(CC) $(CFLAGS_AARCH64) -c -emit-llvm -o $@.bc $<
	$(OPT) $(OPT_FLAGS) $@.bc -o $@.bc
	$(CC) $(CFLAGS_AARCH64) -c -o $@ $@.bc

libbomp_aarch64.a: $(BUILD_AARCH64)/.dir $(OBJ_AARCH64)
	$(AR) -q $@ $(OBJ_AARCH64)

$(BUILD_X86_64)/%.o: %.c
	$(CC) $(CFLAGS_X86_64) -c -emit-llvm -o $@.bc $<
	$(OPT) $(OPT_FLAGS) $@.bc -o $@.bc
	$(CC) $(CFLAGS_X86_64) -c -o $@ $@.bc

libbomp_x86-64.a: $(BUILD_X86_64)/.dir $(OBJ_X86_64)
	$(AR) -q $@ $(OBJ_X86_64)

install: libbomp_aarch64.a libbomp_x86-64.a
	cp libbomp_aarch64.a $(POPCORN)/aarch64/lib/libbomp.a
	cp omp.h $(POPCORN)/aarch64/include
	cp libbomp_x86-64.a $(POPCORN)/x86_64/lib/libbomp.a
	cp omp.h $(POPCORN)/x86_64/include
	cp libbomp_x86-64.a $(POPCORN)/lib/libbomp.a
	cp omp.h $(POPCORN)/include

clean:
	rm -rf libbomp_aarch64.a libbomp_x86-64.a $(BUILD_AARCH64) $(BUILD_X86_64)

.PHONY: all tests clean
