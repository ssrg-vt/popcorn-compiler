#include <elf.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include "syscall.h"
#include "atomic.h"
#include "libc.h"

#include "dynlink.h" /* For struct tlsdesc_relocs */

void __init_tls(size_t *, void **);

static void dummy(void) {}
weak_alias(dummy, _init);

__attribute__((__weak__, __visibility__("hidden")))
extern void (*const __init_array_start)(void), (*const __init_array_end)(void);

static void dummy1(void *p) {}
weak_alias(dummy1, __init_ssp);

#define AUX_COUNT 38

__attribute__((__visibility__("hidden")))
size_t __tlsdesc_static();

void __init_libc(char **envp, char *pn, struct tlsdesc_relocs *tlsdesc_relocs)
{
	size_t i, *auxv, aux[AUX_COUNT] = { 0 };
	__environ = envp;
	for (i=0; envp[i]; i++);
	libc.auxv = auxv = (void *)(envp+i+1);
	for (i=0; auxv[i]; i+=2) if (auxv[i]<AUX_COUNT) aux[auxv[i]] = auxv[i+1];
	__hwcap = aux[AT_HWCAP];
	__sysinfo = aux[AT_SYSINFO];
	libc.page_size = aux[AT_PAGESZ];

	if (!pn) pn = (void*)aux[AT_EXECFN];
	if (!pn) pn = "";
	__progname = __progname_full = pn;
	for (i=0; pn[i]; i++) if (pn[i]=='/') __progname = pn+i+1;

	__init_tls(aux, &tlsdesc_relocs->tls_block);

	if (tlsdesc_relocs != NULL) {
		/*
	 	 * Now that TLS is initialized we can apply TLS based relocations
	 	 * that the dynamic linker would normally do. If these type of
	 	 * relocations apply then tlsdesc_relocs.rel != NULL
	 	 */

		size_t rel_size = tlsdesc_relocs->rel_size;
		size_t *rel = tlsdesc_relocs->rel;
		size_t *reloc_addr;
		size_t sym_index;
		size_t tls_val;
		size_t addend;
		Elf64_Sym *symtab = tlsdesc_relocs->symtab;
		Elf64_Sym *sym;
		for (; rel_size; rel+=3, rel_size-=3*sizeof(size_t)) {
			if (R_TYPE(rel[1]) == REL_TLSDESC) {
				size_t addr = tlsdesc_relocs->base + rel[0];
				reloc_addr = (size_t *)addr;
				sym_index = R_SYM(rel[1]);
				sym = &symtab[sym_index];
				tls_val = sym->st_value;
				addend = rel[2];
				reloc_addr[0] = (size_t)__tlsdesc_static;
				reloc_addr[1] = (size_t)tls_val + addend;
			}
		}
	}

	__init_ssp((void *)aux[AT_RANDOM]);

	if (aux[AT_UID]==aux[AT_EUID] && aux[AT_GID]==aux[AT_EGID]
		&& !aux[AT_SECURE]) return;

	struct pollfd pfd[3] = { {.fd=0}, {.fd=1}, {.fd=2} };
#ifdef SYS_poll
	__syscall(SYS_poll, pfd, 3, 0);
#else
	__syscall(SYS_ppoll, pfd, 3, &(struct timespec){0}, 0, _NSIG/8);
#endif
	for (i=0; i<3; i++) if (pfd[i].revents&POLLNVAL)
		if (__sys_open("/dev/null", O_RDWR)<0)
			a_crash();
	libc.secure = 1;
}

static void libc_start_init(void)
{
	_init();
	uintptr_t a = (uintptr_t)&__init_array_start;
	for (; a<(uintptr_t)&__init_array_end; a+=sizeof(void(*)()))
		(*(void (**)(void))a)();
}

weak_alias(libc_start_init, __libc_start_init);

/* Store the highest stack address dedicated to function activations. */
void *__popcorn_stack_base = NULL;

int __libc_start_main(int (*main)(int,char **,char **), int argc, char **argv,
    struct tlsdesc_relocs *tls_relocs)
{
	char **envp = argv+argc+1;
	__popcorn_stack_base = argv;

	__init_libc(envp, argv[0], tls_relocs);
	__libc_start_init();

	/* Pass control to the application */
	exit(main(argc, argv, envp));
	return 0;
}
