#ifndef _MY_BEGIN_H
#define _MY_BEGIN_H

#include <my_private.h>

Elf* my_read_elf_begin(int fd, Elf_Cmd cmd, Elf *ref);

#endif
