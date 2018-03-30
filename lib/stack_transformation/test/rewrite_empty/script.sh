#! /bin/bash
mkdir -p build_x86-64/
touch build_x86-64/.dir

/usr/local/popcorn/bin/clang -target x86_64-hermit -O0 -Wall -Wno-unused-variable -g -nostdlib -static -I../../utils -DPOSIX -mllvm -optimize-regalloc -isystem /home/mehrab/hermit/x86_64-hermit/include/ -c -emit-llvm -o build_x86-64/rewrite_empty.bc rewrite_empty.c

/usr/local/popcorn/bin/opt -O3 -insert-stackmaps -o build_x86-64/rewrite_empty.bc build_x86-64/rewrite_empty.bc

/usr/local/popcorn/bin/clang -target x86_64-hermit -O0 -Wall -Wno-unused-variable -g -nostdinc -nostdlib -static -I../../utils -DPOSIX -mllvm -optimize-regalloc -o rewrite_empty_x86-64.o -c build_x86-64/rewrite_empty.bc

/home/mehrab/working-binutils/gold/ld-new -T /home/mehrab/3default_linker_script.sc -o rewrite_empty_x86-64 rewrite_empty_x86-64.o /home/mehrab/hermit/x86_64-hermit/lib/crt0.o /home/mehrab/hermit/lib/gcc/x86_64-hermit/6.3.0/libgcc.a /home/mehrab/hermit/lib/gcc/x86_64-hermit/6.3.0/crti.o /home/mehrab/hermit/lib/gcc/x86_64-hermit/6.3.0/crtbegin.o /home/mehrab/hermit/lib/gcc/x86_64-hermit/6.3.0/crtend.o /home/mehrab/hermit/lib/gcc/x86_64-hermit/6.3.0/crtn.o /home/mehrab/hermit/x86_64-hermit/lib/libhermit.a /home/mehrab/hermit/x86_64-hermit/lib/libm.a /home/mehrab/hermit/x86_64-hermit/lib/libpthread.a /home/mehrab/hermit/x86_64-hermit/lib/libc.a ../../build/x86_64/libstack-transform.a /home/mehrab/hermit/x86_64-hermit/lib/libc.a

/usr/local/popcorn/bin/gen-stackinfo -f rewrite_empty_x86-64
