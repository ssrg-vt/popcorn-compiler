## Defining ASLR

ASLR (Address space layout randomization) is a Linux kernel feature which is
able to randomly relocate the stack, the heap, and the base address of loaded
modules in order to harden the attack surface against various types of
memory corruption attacks. The unpredictability of the address space in
a vulnerable program makes exploitation much harder.


Popcorn Linux currently uses statically linked ELF executable files, which are
created using the heavily modified popcorn-compiler toolchain.  Historically in
Linux a statically linked executable can only be built as ET_EXEC type ELF
files. These are standard executable's that only work with an absolute address
space. In-fact dynamically linked executable could only be linked as ET_EXEC
file's too.  Traditionally the ET_DYN file types were reserved for shared
libaries. This ET_DYN file type typically has a base address of 0 on disk, so
that it can be randomly relocated at runtime by the kernel. For along time
shared libraries were the only type of modules that could have their base
randomized at load-time with ASLR. The ET_DYN type file is built with position
independent code and has a base text-address of 0 on disk.  Eventually Linux
security folks, namely Pipacs (Author of PaX), realized that dynamically linked
executable files can be compiled and linked into a position independent ET_DYN
binary that works with ASLR.  So that both shared libraries and dynamically
linked executables could reap the benefits of ASLR.

The ability to build a dynamically linked executable with ASLR compatibility
became known as "Position independent executable" or simply "PIE".  This was
eventually built into the gcc compiler as '-fPIC -pie' which builds a dynamically
linked PIE binary that is compatible with ASLR.

So if shared libraries and dynamically linked executables can be relocated at
runtime via ASLR for security measures. What though about statically linked
executable? The command 'gcc -fPIC -pie -static' was considered until just
recently an erroneous combination of flags, and building statically linked PIE
executable's was not possible. In 2017/2018 I published several papers that
released prototypes for getting static binaries to work with ASLR. Shortly
following the release of those papers I noticed that there was a new
'-static-pie' flag shipped with gcc.  In the present day it is possible to get
static executable's compiled and linked as position independent executable's.
Now there exists a '-static-pie' flag within gcc and clang. The 'static-pie'
flag creates a statically linked executable that is essentially built just like
a shared library.

### Characteristics of a static PIE executable

1. The ELF file type is ET_DYN
2. The base address of the first PT_LOAD segment is 0
3. All of the code is PIC (Position independent code)
4. Must include initilization code for handling relative relocations, i.e R_X86_64_RELATIVE
5. Must include initilization code for handling TLS relocations

### static PIE binaries in heterogenious Popcorn Linux


Popcorn Linux has quite a number of caveats that make getting ASLR to work
challenging. The least of which is that only static binaries are supported by
Popcorn. Static binaries as aformentioned can be built with a '-static-pie'
flag, however this will not fly for Popcorn binaries. The Popcorn compiler
toolchain is tricky, heavily modified, and utilizes a number of compilers (gcc,
musl-gcc, clang), linker-scripts, alignment tools, and custom stack unwinding
and call-site meta-data tooling, which together all build the final output
binaries for each node (i.e. one x86_64 and the other aarch64).

In short, getting ASLR working with Popcorn Linux is a tedious process of
reverse engineering and developing fairly esoteric workarounds, code-patches,
and tools. In the current state of affairs we have Popcorn binaries working
with ASLR up until migration, at which point a crash happens after the successful
migration of the source address space to the destination.

Let us clearly define what the first-phase of ASLR should look like. We call it
load-time ASLR.  On node-A we run 'test_x86_64' which properly executes with a
randomized address base of the test_x86_64 executable. Upon migration, the
kernel instructs the executable on node-B 'test_aarch64' to be loaded into
memory at the exact same virtual address range as on node-A. So the binaries on
each node are being randomly loaded to the same address space. This behavior is
essentially the way ASLR works on any Linux system with the exception that we
are migrating this randomized address space to a destination node for
heterogenious execution.

### The challenges to get ASLR working

As I mentioned previously, Popcorn binaries are statically linked. They are
statically linked with a custom set of build tools, a modified musl-libc, 
and a modified LLVM among other things. The Makefile for building a Popcorn
test binary is extremely custom. We cannot simply use 'clang --static-pie'
to accomplish our goal. Our goal is to build a popcorn binary that is compiled
with PIC code and is linked into an ET_DYN type ELF file with a base address
of 0.

### Building each library with -fPIC

Before we attempt building and linking a PIE binary, the libraries need to be
compiled with -fPIC to make them position independent. This includes:

lib/libelf
lib/migration
lib/stack_depth
lib/stack_transformation
lib/migration
lib/musl-1.1.18

We modified the popcorn-compiler/install_compiler.py script so that it uses the
flags '-fPIC -DPIC' on each library.

Upon building these we came accross several errors that were preventing libstack-transform
and libmigration from compiling with -fPIC. These errors came down to several macros that
were using inline assembly which required relocations that were not compatible with
PIC code. We modified these small areas of assembly so that the libraries would compile
with PIC code.

For example:

```
diff --git a/lib/migration/include/arch/x86_64/migrate.h b/lib/migration/include/arch/x86_64/migrate.h
index 894eecd8..734bb809 100644
--- a/lib/migration/include/arch/x86_64/migrate.h
+++ b/lib/migration/include/arch/x86_64/migrate.h
@@ -71,14 +71,14 @@
       } \
       else \
       { \
-        asm volatile ("movq $1f, %0;" \
+        asm volatile( "leaq .Lmigrate(%%rip), %%rdi;" \
                       "movl %2, %%edi;" \
                       "movq %3, %%rsi;" \
                       "movq %4, %%rsp;" \
                       "movq %5, %%rbp;" \
                       "movl %6, %%eax;" \
                       "syscall;" \
-                      "1: movl %%eax, %1;" \
+                     ".Lmigrate: movl %%eax, %1;" \
                       : /* Outputs */ \
                       "=m"(data.post_syscall), "=g"(err) \
                       : /* Inputs */ \
```

Another example from libstack-transform

```
+ * NOTE: This version of GET_RIP is not suitable for creating
+ * PIE binaries, because it creates a sign-extension relocation
+ * that isn't compatible. Since we need PIE for ASLR we must
+ * use a PIC compatible version of the macro.
+ *
 #define GET_RIP( var ) asm volatile("movq $., %0" : "=g" (var) )
-
+ *
+ * The following version is suitable for linking into a PIE
+ * binary, but is causing a strange compilation error
+ */
+//#define GET_RIP(in) asm volatile("leaq (%%rip), %0" : "=g"(in))
+ /* For now we use this:
+  */
+#define GET_RIP(in) asm volatile("call get_rip          \n" \
+                                 "get_rip:              \n" \
+                                 "pop %0                \n" : "=g"(in));
```

After these changes we should be able to modify the Makefile to build
our test Popcorn binary as a static-pie binary. Unfortunately we cannot
just use the -static-pie flag in our Makefile, there is way too many
nuances that will prevent this from working correctly with the Gold linker.
We also tried the BFD linker and ran into issues as well.

### Modifying the Makefile for the test binary

We must add -fPIC to the CFLAGS, and -shared -Bsymbolic to the LDFLAGS.
This is essentially how a shared library is built. Our final executable
file should result in an ELF file that's virtually identical to a shared
libary. There is very little difference between a PIE executable and a
shared library. They are both executable files with PIC code that can be
relocated to a random base address, hence ASLR.

If we run our new test executable it will crash within the musl-libc initialization
code. A quick glance at the Makefile will show that we are still using the crt1.o
initialization object. We must replace this with rcrt.o, which is necessary so that
the `__dls2()` function is called for handling various runtime relocations.

Another attempt to run our test binary also results in a crash, this time within
`_dlstart_c`. This function handles quite a bit of initial heavy lifting. It parses
the auxv (auxiliary vector), and several program header values to calculate the
base address of our executable at runtime. 
