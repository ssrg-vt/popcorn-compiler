#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <platform.h>
#include "libc.h"
#include "syscall.h"
#include "atomic.h"
#include "pthread_impl.h"

/* This function returns true if the interval [old,new]
 * intersects the 'len'-sized interval below &libc.auxv
 * (interpreted as the main-thread stack) or below &b
 * (the current stack). It is used to defend against
 * buggy brk implementations that can cross the stack. */

static int traverses_stack_p(uintptr_t old, uintptr_t new)
{
	const uintptr_t len = 8<<20;
	uintptr_t a, b;

	b = (uintptr_t)libc.auxv;
	a = b > len ? b-len : 0;
	if (new>a && old<b) return 1;

	b = (uintptr_t)&b;
	a = b > len ? b-len : 0;
	if (new>a && old<b) return 1;

	return 0;
}

void *__mmap(void *, size_t, int, int, int, off_t);

/* Expand the heap in-place if brk can be used, or otherwise via mmap,
 * using an exponential lower bound on growth by mmap to make
 * fragmentation asymptotically irrelevant. The size argument is both
 * an input and an output, since the caller needs to know the size
 * allocated, which will be larger than requested due to page alignment
 * and mmap minimum size rules. The caller is responsible for locking
 * to prevent concurrent calls. */

void *__expand_heap(size_t *pn)
{
	static uintptr_t brk;
	static unsigned mmap_step;
	size_t n = *pn;

	if (n > SIZE_MAX/2 - PAGE_SIZE) {
		errno = ENOMEM;
		return 0;
	}
	n += -n & PAGE_SIZE-1;

	if (!brk) {
		brk = __syscall(SYS_brk, 0);
		brk += -brk & PAGE_SIZE-1;
	}

	if (n < SIZE_MAX-brk && !traverses_stack_p(brk, brk+n)
	    && __syscall(SYS_brk, brk+n)==brk+n) {
		*pn = n;
		brk += n;
		return (void *)(brk-n);
	}

	size_t min = (size_t)PAGE_SIZE << mmap_step/2;
	if (n < min) n = min;
	void *area = __mmap(0, n, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (area == MAP_FAILED) return 0;
	*pn = n;
	mmap_step++;
	return area;
}

/* In Popcorn, reduce cross-node interference by using a per-node heap
 * allocated via mmap (avoid using sbrk altogether).  Logically allocate a
 * large chunk of the virtual address space to each node and mmap/mremap to
 * expand the heap. */

#define ARENA_SIZE (1ULL << 30ULL)
#define ARENA_START(base, nid) ((void *)((base) + ((nid) * ARENA_SIZE)))
#define ARENA_CONTAINS(base, nid, ptr) \
	(ARENA_START(base, nid) <= (ptr) && (ptr) < (ARENA_START(base, nid + 1)))

static uintptr_t arena_start;

/* Set the start of the per-thread arenas. Gives the regular heap space
 * in case the user is mixing regular & Popcorn allocations. */
static inline void set_arena_start()
{
	static int lock[2];

	if (libc.threads_minus_1)
		while(a_swap(lock, 1)) __wait(lock, lock+1, 1, 1);

	if (!arena_start) {
		arena_start = __syscall(SYS_brk, 0);
		arena_start += -arena_start & PAGE_SIZE-1;
		arena_start += 4 * ARENA_SIZE;
	}

	if (lock[0]) {
		a_store(lock, 0);
		if (lock[1]) __wake(lock, 1, 1);
	}
}

void *__mremap(void *, size_t, size_t, int, ...);

void *__expand_heap_node(size_t *pn, int nid)
{
	// We don't use the program break other than to set the starting address for
	// the arenas.  The node_arenas array contains the current "break" for a
	// node, or zero if uninitialized; ARENA_START should be used in that case.
	// The node_sizes array contains the current heap size for a node.
	static uintptr_t node_arenas[MAX_POPCORN_NODES];
	static size_t node_sizes[MAX_POPCORN_NODES];
	size_t n = *pn;
	void *area;

	if(nid < 0 || nid >= MAX_POPCORN_NODES) {
		errno = EINVAL;
		return 0;
	}

	if (n > SIZE_MAX/2 - PAGE_SIZE) {
		errno = ENOMEM;
		return 0;
	}
	n += -n & PAGE_SIZE-1;

	if(!arena_start) set_arena_start();

	if((node_sizes[nid] + n) <= ARENA_SIZE) {
		// TODO Popcorn Linux doesn't currently support mremap.  Linux *shouldn't*
		// allocate physical pages to mmap'd regions until we touch them, so just
		// map in the entire arena.  Note that we should *never* re-enter here, as
		// the if-statement that guards this path should hereafter be false.
		n = ARENA_SIZE;

		if(!node_arenas[nid]) {
			area = __mmap(ARENA_START(arena_start, nid), n, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
			if(area == MAP_FAILED) return 0;
			node_arenas[nid] = (uintptr_t)area + n;
		}
		else {
			area = __mremap(ARENA_START(arena_start, nid), node_sizes[nid],
					node_sizes[nid] + n, 0);
			if(area == MAP_FAILED) return 0;
			node_arenas[nid] += n;
		}
		node_sizes[nid] += n;
		*pn = n;
		return (void *)(node_arenas[nid] - n);
	}

	// TODO without extra metadata we can't detect which arena malloc'd space
	// belongs with anonymous mmaps, so for now just bail.
	*pn = 0;
	return NULL;
}

int popcorn_get_arena(void *ptr)
{
	int arena;
	if (!arena_start) set_arena_start();
	arena = (int)(((uintptr_t)ptr - arena_start) / ARENA_SIZE);
	if(arena >= MAX_POPCORN_NODES) arena = -1;
	return arena;
}
