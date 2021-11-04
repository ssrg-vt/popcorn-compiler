## defining aslr

aslr (address space layout randomization) is a linux kernel feature which is
able to randomly relocate the stack, the heap, and the base address of loaded
modules in order to harden the attack surface against various types of
memory corruption attacks. the unpredictability of the address space in
a vulnerable program makes exploitation much harder.


popcorn linux currently uses statically linked elf executable files, which are
created using the heavily modified popcorn-compiler toolchain.  historically in
linux a statically linked executable can only be built as et_exec type elf
files. these are standard executable's that only work with an absolute address
space. in-fact dynamically linked executable could only be linked as et_exec
file's too.  traditionally the et_dyn file types were reserved for shared
libaries. this et_dyn file type typically has a base address of 0 on disk, so
that it can be randomly relocated at runtime by the kernel. for along time
shared libraries were the only type of modules that could have their base
randomized at load-time with aslr. the et_dyn type file is built with position
independent code and has a base text-address of 0 on disk.  eventually linux
security folks, namely pipacs (author of pax), realized that dynamically linked
executable files can be compiled and linked into a position independent et_dyn
binary that works with aslr.  so that both shared libraries and dynamically
linked executables could reap the benefits of aslr.

the ability to build a dynamically linked executable with aslr compatibility
became known as "position independent executable" or simply "pie".  this was
eventually built into the gcc compiler as '-fpic -pie' which builds a dynamically
linked pie binary that is compatible with aslr.

so if shared libraries and dynamically linked executables can be relocated at
runtime via aslr for security measures. what though about statically linked
executable? the command 'gcc -fpic -pie -static' was considered until just
recently an erroneous combination of flags, and building statically linked pie
executable's was not possible. in 2017/2018 i published several papers that
released prototypes for getting static binaries to work with aslr. shortly
following the release of those papers i noticed that there was a new
'-static-pie' flag shipped with gcc.  in the present day it is possible to get
static executable's compiled and linked as position independent executable's.
now there exists a '-static-pie' flag within gcc and clang. the 'static-pie'
flag creates a statically linked executable that is essentially built just like
a shared library.

### characteristics of a static pie executable

1. the elf file type is et_dyn
2. the base address of the first pt_load segment is 0
3. all of the code is pic (position independent code)
4. must include initilization code for handling relative relocations, i.e r_x86_64_relative
5. must include initilization code for handling tls relocations

### static pie binaries in heterogenious popcorn linux


popcorn linux has quite a number of caveats that make getting aslr to work
challenging. the least of which is that only static binaries are supported by
popcorn. static binaries as aformentioned can be built with a '-static-pie'
flag, however this will not fly for popcorn binaries. the popcorn compiler
toolchain is tricky, heavily modified, and utilizes a number of compilers (gcc,
musl-gcc, clang), linker-scripts, alignment tools, and custom stack unwinding
and call-site meta-data tooling, which together all build the final output
binaries for each node (i.e. one x86_64 and the other aarch64).

in short, getting aslr working with popcorn linux is a tedious process of
reverse engineering and developing fairly esoteric workarounds, code-patches,
and tools. in the current state of affairs we have popcorn binaries working
with aslr up until migration, at which point a crash happens after the successful
migration of the source address space to the destination.

let us clearly define what the first-phase of aslr should look like. we call it
load-time aslr.  on node-a we run 'test_x86_64' which properly executes with a
randomized address base of the test_x86_64 executable. upon migration, the
kernel instructs the executable on node-b 'test_aarch64' to be loaded into
memory at the exact same virtual address range as on node-a. so the binaries on
each node are being randomly loaded to the same address space. this behavior is
essentially the way aslr works on any linux system with the exception that we
are migrating this randomized address space to a destination node for
heterogenious execution.

### the challenges to get aslr working

as i mentioned previously, popcorn binaries are statically linked. they are
statically linked with a custom set of build tools, a modified musl-libc, 
and a modified llvm among other things. the makefile for building a popcorn
test binary is extremely custom. we cannot simply use 'clang --static-pie'
to accomplish our goal. our goal is to build a popcorn binary that is compiled
with pic code and is linked into an et_dyn type elf file with a base address
of 0.

### building each library with -fpic

before we attempt building and linking a pie binary, the libraries need to be
compiled with -fpic to make them position independent. this includes:

lib/libelf
lib/migration
lib/stack_depth
lib/stack_transformation
lib/migration
lib/musl-1.1.18

we modified the popcorn-compiler/install_compiler.py script so that it uses the
flags '-fpic -dpic' on each library.

upon building these we came accross several errors that were preventing libstack-transform
and libmigration from compiling with -fpic. these errors came down to several macros that
were using inline assembly which required relocations that were not compatible with
pic code. we modified these small areas of assembly so that the libraries would compile
with pic code.

for example:

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
+        asm volatile( "leaq .lmigrate(%%rip), %%rdi;" \
                       "movl %2, %%edi;" \
                       "movq %3, %%rsi;" \
                       "movq %4, %%rsp;" \
                       "movq %5, %%rbp;" \
                       "movl %6, %%eax;" \
                       "syscall;" \
-                      "1: movl %%eax, %1;" \
+                     ".lmigrate: movl %%eax, %1;" \
                       : /* outputs */ \
                       "=m"(data.post_syscall), "=g"(err) \
                       : /* inputs */ \
```

another example from libstack-transform

```
+ * note: this version of get_rip is not suitable for creating
+ * pie binaries, because it creates a sign-extension relocation
+ * that isn't compatible. since we need pie for aslr we must
+ * use a pic compatible version of the macro.
+ *
 #define get_rip( var ) asm volatile("movq $., %0" : "=g" (var) )
-
+ *
+ * the following version is suitable for linking into a pie
+ * binary, but is causing a strange compilation error
+ */
+//#define get_rip(in) asm volatile("leaq (%%rip), %0" : "=g"(in))
+ /* for now we use this:
+  */
+#define get_rip(in) asm volatile("call get_rip          \n" \
+                                 "get_rip:              \n" \
+                                 "pop %0                \n" : "=g"(in));
```

after these changes we should be able to modify the makefile to build
our test popcorn binary as a static-pie binary. unfortunately we cannot
just use the -static-pie flag in our makefile, there is way too many
nuances that will prevent this from working correctly with the gold linker.
we also tried the bfd linker and ran into issues as well.

One issue that we ran into was within the tool/alignment/pyalign tool
which is invoked by the Makefile to enforce proper alignment on the
output binary. Each library is being built as a static PIC library,
who's object files are stored within a final archive file, i.e. libstacktransform.a
-- however the object files within the archive file take on the suffix of
'.lo' instead of '.o' when building PIC code. The pyalign tool could not
handle this, thus a patch to tool/alignment/Symbol.py

```
 def symbolObjectFileSanityCheck(obj):
-       reLib = "^(.+\.a)\((.+\.o)\)" # To check if it comes from an archive
+        print obj
+        reLib = "^(.+\.a)\((.+\.l?o)\)"
        reObj = "^(.+\.o)"                              # or an object file
```

### debugging our Makefile and musl-libc


in our test binary makefile we must add -fpic to the cflags, and -shared
-bsymbolic to the ldflags.  this is essentially how a shared library is built.
our final executable file should result in an elf file that's virtually
identical to a shared libary. there is very little difference between a pie
executable and a shared library. they are both executable files with pic code
that can be relocated to a random base address, hence aslr.

if we run our new test executable it will crash within the musl-libc
initialization code. a quick glance at the makefile will show that we are still
using the crt1.o initialization object. we must replace this with rcrt.o, which
is necessary so that the `__dls2()` function is called for handling various
runtime relocations.

another attempt to run our test binary also results in a crash, this time
within `_dlstart_c`. this function handles quite a bit of initial heavy
lifting. it parses the auxv (auxiliary vector), and calculates the base address
of our binary by subtracting the `pt_dynamic` phdr value from the runtime
address of `_dynamic`. unfortunately for us these calculations seem to be
broken; a quick look at the program header table of our test binary will tell
us why...

```
elf file type is dyn (shared object file)
entry point 0x521c98
there are 13 program headers, starting at offset 64

program headers:
  type           offset             virtaddr           physaddr
                 filesiz            memsiz              flags  align
  phdr           0x0000000000000040 0x00000000003ff040 0x00000000003ff040
                 0x00000000000002d8 0x00000000000002d8  r      0x8
  load           0x0000000000000000 0x00000000003ff000 0x00000000003ff000
                 0x0000000000000318 0x0000000000000318  r      0x1000
```

the virtual address for the first load segment is not zero like we would expect
to see in an et_dyn shared object. the first load segment should have a base
address of 0x00000000, and all following load segments should have virtual
addresses that are offsets from the base address. this idiosyncrasy is causing
an error in the musl-libc code that performs calculations on these phdr values.
to correct this i first tried many different ways to tweak the linker into
creating a proper static pie executable with a base address of zero. modifying
the template linker scripts to force a text address of zero had failed results
with the gold linker and resulted in a binary who's pt_phdr had a base address
of zero, but the first pt_load phdr had a base address slightly above zero. a
quick look at the gold linker code proved a rabbit hole. i instead opted to
design a workaround that entails modifications to musl-libc and to various elf
meta-data in the binary itself.


Our static PIE binary having a base address larger than 0 is a problem.  At
runtime the kernel randomly relocates the base address of a PIE binary to
something random, say 0x7fff2a0000. All of the musl-libc initialization code is
expecting all of the program headers and relocation records to have addresses
that are offsets from the base address of zero. This way it can calculate these
offsets from the base address 0x7fff2a0000 and get the correct memory addresses
for initilization calcluations and relocation. Again, all of the program header
p_vaddr values should be offsets from 0 in order to be calculated into the final
virtual address at runtime, after ASLR has chosen a base address. Our somewhat
defected PIE binaries are linked with fixed virtual addresses.

musl-libc is not able to calculate the correct base address of our binary at
runtime without having a base PT_LOAD address of zero. We workaround this with
a patch to musl-libc/ldso/dlstart.c

```
@@ -106,13 +117,41 @@ void _dlstart_c(size_t *sp, size_t *dynv)
                size_t phnum = aux[AT_PHNUM];
                size_t phentsize = aux[AT_PHENT];
                Phdr *ph = (void *)aux[AT_PHDR];
+               int c = 0;
+               int interp_exists = 0;
+               int popcorn_aslr = 0;
+               size_t first_load_addr;
+               /*
+                * For our somewhat obscure purposes, and since we
+                * can't yet get the initial ELF hdr, we can deduce
+                * that this is a "Popcorn PIE" binary if it has no
+                * PT_INTERP, a base PT_LOAD address greater than 0,
+                * and a PT_DYNAMIC segment.
+                */
+               for (i=0; i < phnum; i++) {
+                       if (ph[i].p_type == PT_LOAD && c == 0) {
+                               first_load_addr = ph[i].p_vaddr;
+                               c++;
+                       } else if (ph[i].p_type == PT_INTERP) {
+                               interp_exists = 1;
+                       } else if (ph[i].p_type == PT_DYNAMIC) {
+                               if (first_load_addr > 0 && interp_exists == 0) {
+                                       popcorn_aslr = 1;
+                               }
+                       }
+               }
                for (i=phnum; i--; ph = (void *)((char *)ph + phentsize)) {
                        if (ph->p_type == PT_DYNAMIC) {
-                               base = (size_t)dynv - ph->p_vaddr;
+                               if (popcorn_aslr > 0) {
+                                       base = (size_t)dynv - (ph->p_vaddr - first_load_addr);
+                               } else {
+                                       base = (size_t)dynv - ph->p_vaddr;
+                               }
                                break;
                        }
                }
        }
```

It is important to note that in a separate patch to this file we introduce
`void *__popcorn_text_base;` as a global symbol. This value is set to the
calculated runtime base address of our binary. This base address is later
passed along to libstacktransform and libmigrate which have been modified
to deal with offsets instead of absolute addresses. The offsets are of course
added to the base `void *__popcorn_text_base` to achieve the correct addresses
after ASLR has been applied. 


### musl-libc relative relocations

After our patch to musl-libc that calculates the correct base address at
runtime, we have made some progress, but we were still crashing within
the `_dlstart_c` function somewhere. This was due to the following code

```
        rel_size = dyn[DT_RELASZ];
        for (; rel_size; rel+=3, rel_size-=3*sizeof(size_t)) {
                if (IS_RELATIVE(rel[1], 0)) {
                        size_t *rel_addr = (void *)(base + rel[0]);
                        *rel_addr = base + rel[2];
                }
        }
```

The relocation requires adding the base address of the binary at runtime to
`rel[0]` (which is the same as `rel->r_offset`). So adding the relocation
offset to the base address computes the relocation target address. The
relocation target is then patched with the correct relocation computation and
the relocation has then been carried out. In our case adding `base +
rel->r_offset` computes the incorrect relocation target due to the fact that
all of the relocation records are storing an `r_offset` value that is not an
offset from 0, but rather an absolute address that corresponds to the first
PT_LOAD address in our quirky binary. Again take a quick look at the initial
PT_LOAD segment in the following readelf output. The musl-libc relocation
parsing code expects all of the `r_offset` values in the R_RELATIVE relocation
records to be offsets from 0 instead of virtual addresses that come after the
PT_LOAD value 0x3ff000.

```
Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  PHDR           0x0000000000000040 0x00000000003ff040 0x00000000003ff040
                 0x00000000000002d8 0x00000000000002d8  R      0x8
  LOAD           0x0000000000000000 0x00000000003ff000 0x00000000003ff000
                 0x0000000000000318 0x0000000000000318  R      0x1000
  LOAD           0x0000000000001190 0x0000000000400190 0x0000000000400190
                 0x0000000000005d13 0x0000000000005d13  R      0x1000
  LOAD           0x0000000000100000 0x0000000000500000 0x0000000000500000
                 0x00000000000232a0 0x00000000000232a0  R E    0x100000
  LOAD           0x0000000000200000 0x0000000000600000 0x0000000000600000
                 0x0000000000003f10 0x0000000000003f10  RW     0x100000
```

A quick look at a few of the relative relocation records will show that
their `r_offset values` correspond to the data segment (The 4th) PT_LOAD
segment which has a virtual address of 0x600000.

```
Relocation section '.rela.dyn' at offset 0x121cb0 contains 234 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
0000006030d0  000000000008 R_X86_64_RELATIVE                    50fa50
0000006030d8  000000000008 R_X86_64_RELATIVE                    50fa50
0000006030e0  000000000008 R_X86_64_RELATIVE                    50fb40
```

When adding an r_offset of 0x6030d0 to the base address calculated by the kernel
at runtime, we will achieve an incorrect address to perform relocations. We must
determine the offset of 0x6030d0 from the beginning of the text segment, and then
add the offset to the base runtime address.

Subtract the first PT_LOAD address `0x3ff000` from the r_offset `0x6030d0`
value seen in the relocation output above to acquire the offset that the
relocation target lives at. This offset can be added to the runtime base address
denoted by `void *__popcorn_text_base` to acquire the address of the relocation
target.

In order to solve this problem I created the
`popcorn-compiler/test_aslr/patch_elf.c` tool which reconstructs various parts
of the ELF meta-data in our binary to be adjusted from absolute addresses to
offsets. Again the musl-libc initialization code will attempt to add absolute
addresses to the base runtime address resulting in all kinds of errors.  To fix
this we must convert absolute addresses to offsets within the relocation
records and within various dynamic segment values. We patch the binary directly
to adjust these values.

```
        char *StringTable = (char *)&mem[shdr[ehdr->e_shstrndx].sh_offset];
        Elf64_Rela *rela;
        for (i = 0; i < ehdr->e_shnum; i++) {
                if (strcmp(&StringTable[shdr[i].sh_name], ".rela.dyn") != 0)
                        continue;
                printf("Found .rela.dyn section\n");
                printf("Patching relocation entries with updated r_offset's\n");
                rela = (Elf64_Rela *)&mem[shdr[i].sh_offset];
                for (j = 0; j < shdr[i].sh_size / shdr[i].sh_entsize; j++) {
                        if (ehdr->e_machine == EM_X86_64) {
                                if (ELF64_R_TYPE(rela[j].r_info) == R_X86_64_DTPMOD64)
                                        continue;
                        }
                        printf("Changing %#lx to %#lx\n", rela[j].r_offset,
                            rela[j].r_offset - base);
                        rela[j].r_offset -= base;
                        rela[j].r_addend -= base;
                }
        }
```

### musl-libc TLS with PIE binaries

TLS (Thread local storage) is obviously very significant to our work since we
are dealing with threads. The TLS in Popcorn binaries typically works by
accessing the %fs:0x0 segment register to acquire the TLS block address, which
is then computed with a given offset to a TLS variable. This type of TLS access
is referred to as the 'local-exec' model. As it turns out, when we build our
Popcorn binary as PIE (Position independent executable) a totally different TLS
model is used, referred to as 'local-dynamic'. Furthermore the underlying
mechanics for this TLS model are almost completely different on x86_64 vs.
aarch64.

Let's take a look at the x86_64 TLS code for a regular Popcorn static executable.
```
  505e3f:       66 66 66 64 48 8b 04    data16 data16 data16 mov %fs:0x0,%rax
  505e46:       25 00 00 00 00 
  505e4b:       48 8b 80 10 11 10 00    mov    0x101110(%rax),%rax
  505e52:       48 83 f8 00             cmp    $0x0,%rax
  505e56:       0f 85 2a 00 00 00       jne    505e86 <get_stack_bounds+0x56>
  505e5c:       66 66 66 64 48 8b 04    data16 data16 data16 mov %fs:0x0,%rax
  505e63:       25 00 00 00 00 
  505e68:       48 8d b8 10 11 10 00    lea    0x101110(%rax),%rdi
```

Now let us take a look at the corresponding TLS code within our static PIE
version of the Popcorn binary.

```
50660f:       48 8d 3d c2 ab 4f 00    lea    0x4fabc2(%rip),%rdi        # a011d8 <_DYNAMIC+0x1a8>
  506616:       e8 5d d7 ff ff          callq  503d78 <__tls_get_addr>
  50661b:       48 8b 80 00 10 00 00    mov    0x1000(%rax),%rax
  506622:       48 83 f8 00             cmp    $0x0,%rax
  506626:       0f 85 2a 00 00 00       jne    506656 <get_stack_bounds+0x56>
  50662c:       48 8d 3d a5 ab 4f 00    lea    0x4faba5(%rip),%rdi        # a011d8 <_DYNAMIC+0x1a8>
```






