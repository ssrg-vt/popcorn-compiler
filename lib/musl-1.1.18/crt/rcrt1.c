#define START "_start"
#define _dlstart_c _start_c
#include "../ldso/dlstart.c"

int main();
void _init() __attribute__((weak));
void _fini() __attribute__((weak));
_Noreturn int __libc_start_main(int (*)(), int, char **,
	struct tlsdesc_relocs *tls_relocs, void (*)(), void(*)(), void(*)());

__attribute__((__visibility__("hidden")))
_Noreturn void __dls2(unsigned char *base, size_t *sp, struct tlsdesc_relocs *tls_relocs)
{
	dprintf(1, "In __dls2() tls_relocs->rel: %p\n", tls_relocs->rel);
	__libc_start_main(main, *sp, (void *)(sp+1), tls_relocs, _init, _fini, 0);
}
