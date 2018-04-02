#! /bin/bash

GOLD=/home/pierre/Desktop/hermit-llvm/hermit-toolchain/build/binutils/gold/ld-new

mkdir -p build_x86-64/
touch build_x86-64/.dir

/usr/local/popcorn/bin/clang -target x86_64-hermit -O0 -Wall -Wno-unused-variable -g -nostdlib -static -I../../utils -DPOSIX -mllvm -optimize-regalloc -isystem /home/usr/hermit/x86_64-hermit/include/ -c -emit-llvm -o build_x86-64/rewrite_empty.bc rewrite_empty.c

/usr/local/popcorn/bin/opt -O3 -insert-stackmaps -o build_x86-64/rewrite_empty.bc build_x86-64/rewrite_empty.bc

/usr/local/popcorn/bin/clang -target x86_64-hermit -O0 -Wall -Wno-unused-variable -g -nostdinc -nostdlib -static -I../../utils -DPOSIX -mllvm -optimize-regalloc -o rewrite_empty_x86-64.o -c build_x86-64/rewrite_empty.bc

$GOLD -T ls.x -o rewrite_empty_x86-64 rewrite_empty_x86-64.o /home/usr/hermit/x86_64-hermit/lib/crt0.o /home/usr/hermit/lib/gcc/x86_64-hermit/6.3.0/libgcc.a /home/usr/hermit/lib/gcc/x86_64-hermit/6.3.0/crti.o /home/usr/hermit/lib/gcc/x86_64-hermit/6.3.0/crtbegin.o /home/usr/hermit/lib/gcc/x86_64-hermit/6.3.0/crtend.o /home/usr/hermit/lib/gcc/x86_64-hermit/6.3.0/crtn.o /home/usr/hermit/x86_64-hermit/lib/libhermit.a /home/usr/hermit/x86_64-hermit/lib/libm.a /home/usr/hermit/x86_64-hermit/lib/libpthread.a /home/usr/hermit/x86_64-hermit/lib/libc.a ../../build/x86_64/libstack-transform.a /home/usr/hermit/x86_64-hermit/lib/libc.a

/usr/local/popcorn/bin/gen-stackinfo -f rewrite_empty_x86-64
