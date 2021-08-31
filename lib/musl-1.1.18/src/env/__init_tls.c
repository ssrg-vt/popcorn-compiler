#include <elf.h>
#include <limits.h>
#include <sys/mman.h>
#include <string.h>
#include <stddef.h>
#include "pthread_impl.h"
#include "libc.h"
#include "atomic.h"
#include "syscall.h"

int __init_tp(void *p)
{
	pthread_t td = p;
	td->self = td;
	int r = __set_thread_area(TP_ADJ(p));
	if (r < 0) return -1;
	if (!r) libc.can_do_threads = 1;
	td->tid = __syscall(SYS_set_tid_address, &td->tid);
	td->locale = &libc.global_locale;
	td->robust_list.head = &td->robust_list.head;
	return 0;
}

static struct builtin_tls {
	char c;
	struct pthread pt;
	void *space[16];
} builtin_tls[1];
#define MIN_TLS_ALIGN offsetof(struct builtin_tls, pt)

static struct tls_module main_tls;

void *__copy_tls(unsigned char *mem, void **tls_block)
{
	pthread_t td;
	struct tls_module *p;
	size_t i;
	void **dtv;

#ifdef TLS_ABOVE_TP
	dtv = (void **)(mem + libc.tls_size) - (libc.tls_cnt + 1);

	mem += -((uintptr_t)mem + sizeof(struct pthread)) & (libc.tls_align-1);
	td = (pthread_t)mem;
	mem += sizeof(struct pthread);

	for (i=1, p=libc.tls_head; p; i++, p=p->next) {
		dtv[i] = mem + p->offset;
		memcpy(dtv[i], p->image, p->len);
		/*
		 * The TLS block address
		 */
		*tls_block = dtv[1];
		dprintf(1, "Setting tlsdesc_relocs.tls_block: to dtv[1]: %p\n", *tls_block);
	}
#else
	dtv = (void **)mem;

	mem += libc.tls_size - sizeof(struct pthread);
	mem -= (uintptr_t)mem & (libc.tls_align-1);
	td = (pthread_t)mem;

	for (i=1, p=libc.tls_head; p; i++, p=p->next) {
		dtv[i] = mem - p->offset;
		memcpy(dtv[i], p->image, p->len);
	}
#endif
	dtv[0] = (void *)libc.tls_cnt;
	td->dtv = td->dtv_copy = dtv;
	return td;
}

#if ULONG_MAX == 0xffffffff
typedef Elf32_Phdr Phdr;
#else
typedef Elf64_Phdr Phdr;
#endif

__attribute__((__weak__, __visibility__("hidden")))
extern const size_t _DYNAMIC[];

static void static_init_tls(size_t *aux, void **tls_block)
{
	unsigned char *p;
	size_t n, i, c;
	size_t phnum = aux[AT_PHNUM];
	Phdr *phdr, *tls_phdr=0;
	size_t base = 0;
	size_t first_load_vaddr = 0;
	void *mem;
	int nonzero_base = 0;
	int popcorn_aslr = 0;
	int interp_exists = 0;

	/*
	 * Is this a popcorn PIE binary? We check to see if it's an ET_DYN that
	 * has a base address greater than 0, with no PT_INTERP segment.  In
	 * the future we could add a PT_NOTE entry that tells us instead. Or
	 * better yet patch the gold linker to create our PIE binaries
	 * correctly :)
	 */
	for (i = 0, c = 0, phdr = (void *)aux[AT_PHDR]; i < phnum; i++) {
		if (phdr[i].p_type == PT_LOAD && c == 0) {
			first_load_vaddr = phdr[i].p_vaddr;
			if (phdr[i].p_vaddr > 0) {
				nonzero_base = 1;
			}
			c++;
		} else if (phdr[i].p_type == PT_INTERP) {
			interp_exists = 1;
		} else if (phdr[i].p_type == PT_DYNAMIC) {
			if (nonzero_base > 0 && interp_exists == 0) {
				popcorn_aslr = 1;
			}
		}
	}
	for (p=(void *)aux[AT_PHDR],n=aux[AT_PHNUM]; n; n--,p+=aux[AT_PHENT]) {
		phdr = (void *)p;
		if (phdr->p_type == PT_PHDR) {
			if (popcorn_aslr > 0) {
				base = aux[AT_PHDR] & ~4095;
			} else {
				base = aux[AT_PHDR] - phdr->p_vaddr;
			}
		}
		if (phdr->p_type == PT_DYNAMIC && _DYNAMIC && !popcorn_aslr)
			base = (size_t)_DYNAMIC - phdr->p_vaddr;
		if (phdr->p_type == PT_TLS)
			tls_phdr = phdr;
	}

	if (tls_phdr) {
		if (popcorn_aslr > 0) {
			main_tls.image =
			    (void *)(base + (tls_phdr->p_vaddr - first_load_vaddr));
		} else {
			main_tls.image = (void *)(base + tls_phdr->p_vaddr);
		}
		main_tls.len = tls_phdr->p_filesz;
		main_tls.size = tls_phdr->p_memsz;
		main_tls.align = tls_phdr->p_align;
		libc.tls_cnt = 1;
		libc.tls_head = &main_tls;
	}

	main_tls.size += (-main_tls.size - (uintptr_t)main_tls.image)
		& (main_tls.align-1);
	if (main_tls.align < MIN_TLS_ALIGN) main_tls.align = MIN_TLS_ALIGN;
#ifndef TLS_ABOVE_TP
#if defined __aarch64__ || defined __powerpc64__ || defined __x86_64__
#error Popcorn: all TLS must be *above* the stack pointer!
#endif
	main_tls.offset = main_tls.size;
#endif

	libc.tls_align = main_tls.align;
	libc.tls_size = 2*sizeof(void *) + sizeof(struct pthread)
		+ main_tls.size + main_tls.align
		+ MIN_TLS_ALIGN-1 & -MIN_TLS_ALIGN;

	if (libc.tls_size > sizeof builtin_tls) {
#ifndef SYS_mmap2
#define SYS_mmap2 SYS_mmap
#endif
		mem = (void *)__syscall(
			SYS_mmap2,
			0, libc.tls_size, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
		/* -4095...-1 cast to void * will crash on dereference anyway,
		 * so don't bloat the init code checking for error codes and
		 * explicitly calling a_crash(). */
	} else {
		mem = builtin_tls;
	}

	/* Failure to initialize thread pointer is always fatal. */
	if (__init_tp(__copy_tls(mem, tls_block)) < 0)
		a_crash();
	dprintf(1, "tls_block: %p\n", *tls_block);
}

weak_alias(static_init_tls, __init_tls);
