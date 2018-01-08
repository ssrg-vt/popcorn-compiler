#ifndef MY_STRPTR_H
#define MY_STRPTR_H
char* my_elf_strptr(Elf *elf, size_t section, size_t offset);   
Elf_Data* my_elf_getdata(Elf_Scn *scn, Elf_Data *data);
#endif
