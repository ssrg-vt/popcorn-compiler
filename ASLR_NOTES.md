# Popcorn Linux ASLR for x86_64 and ARM

## Modifications and patches

As described below, we have multiple patches to various Makefiles, tools,
linker code, linker-scripts, and install files.

### New files

The following patch file patches several files within the binutils-gold
linker source code. It aims to allow compatibility between the flags
-static and -pie

```compiler/patches/binutils-gold/gold_static-pie.patch```

This source code, when compiled, will take a binary file as an argument
and modify its elf_hdr->e_type from ET_EXEC to ET_DYN.

```compiler/tool/elf/mark_elf_dyn.c```

### Modified files

Linker scripts to allow for text-segment virtual address of 0x00000000

```compiler/tool/alignment/templates/ls_x86.template```
```compiler/tool/alignment/templates/ls_arm.template```

Pyalign tool is modified so that an alignment of only 0x1000 is used.

```compiler/tool/alignment/Linker.py```

The installer has been modified to use the -fPIC flag with the configure
scripts for each popcorn library.

```compiler/install_compiler.py```

Makefiles have been modified with the -fPIC flag in CFLAGS

```compiler/lib/stack_transformation/Makefile```
```compiler/lib/migration/Makefile```
```compiler/lib/dsm-prefetch/Makefile```


## Description of problems/solutions

The output executable's for popcorn Linux are currently statically linked.
Although the -static-pie option is a flag for the regular dynamic linker,
it is not supported by the gold linker. We have patched the Gold linker
to allow for both the -static and the -pie option simultaneously, but still
have one issue that's not allowing them to work together; the libmigrate.a
code is being built with -fPIC, but the gold linker is still complaining about
a sign-extension relocation for the symbol ```__internal_shim_i```.

Patch to gold linker for -static -pie: ```compiler/patches/binutils-gold/gold_static-pie.patch```

### Workaround avoiding -static -pie flags

To work around this issue I have ignored using -static -pie for now, and built
all of the relocatable objects for each library with -fPIC. I modify the
```build/share/align-script-templates/ls_<arch>.template``` linker script files so
that they begin the text-segment address at 0x00000000. All of these efforts
combined still result in a target executable that begins at address 0xff000,
due to different alignment between multiple LOAD segments, the first segments
alignment is at 0x1000 and others are at 0x100000. To correct this issue I
modified the python file ```compiler/tool/alignment/Linker.py``` which is responsible
for writing out the final linker-script that is used. It now sets all of the
p_align values to 0x1000.

After the statically-linked executable is produced, and has a text-segment
address starting at 0x0, we must mark the ELF header type from ET_EXEC to
ET_DYN so that the kernel knows to randomly relocate the base address of
the executable at runtime. The source file ```compiler/tool/elf/mark_elf_dyn.c```
performs this task and can be ran against a newly created popcorn binary
to make it dynamic.

## ASLR results with Popcorn binary

At this point in time, the aforementioned steps accomplish getting the popcorn
ELF binary loaded at a random address space, and executing up until a segfault
happens in vfprintf by musl-libc. It is not yet clear if this problem is due
to position-dependent code that is somehow still being compiled into musl-libc
or something else. So far only the x86_64 popcorn binary has been tested. The
ARM binary should work as well as its linked the same way, however we won't
know until we try running it in a popcorn instance that has the ARM migration
setup.

## Steps for successful ASLR in popcorn binaries

1. we must build all dependencies with position independent code, all object
files must be built with the -fPIC flag.

2. The crt1.o (i.e. glibc initialization) must be suitable for a position
independent executable, I believe we need to be using either Scrt1.o or
rcrt1.o

3. The output executable must be must be dynamic (Marked with an ELF type of ET_DYN)

4. The output executable must have a text segment that starts at vaddr 0x00000000

5. The pyalign tool must be modified so that it does not use huge PAGE Alignment of 0x100000
for some segments, and 0x1000 for others. This has been modified so that all segment aligns
are set to 0x1000. If this causes issues perhaps we can change all aligns to 0x100000, which
was originally used to account for padding and alignment when ARM instructions take up more
space than the corresponding x86 instruction.

## Alternative techniques we've tried

We've designed a program based on my research in 2018 on getting ASLR to work with static
executables (Prior to the gcc -static-pie flag existing). We ran into similar issues as
with our approaches covered above. Primarily that the object files for the libraries still
appear to be running into position-dependent code of some sort

We decided that it would be more appropriate to build the capabilities for making static-pie
popcorn binaries right into the popcorn-compiler toolchain. We may resort to this binary
modification approach as outlined in my paper if we need to.

Paper here: https://www.leviathansecurity.com/blog/aslr-protection-for-statically-linked-executables



