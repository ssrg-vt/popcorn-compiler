#include "futex.h"
#include "syscall.h"
#include "libc.h"

int __futex(volatile int *addr, int op, int val, void *ts)
{
	// TODO Popcorn: we don't currently support process-shared futexes
	op |= FUTEX_PRIVATE;
	return syscall(SYS_futex, addr, op, val, ts);
}

weak_alias(__futex, futex);
