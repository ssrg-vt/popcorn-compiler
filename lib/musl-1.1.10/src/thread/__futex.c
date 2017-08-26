#include "futex.h"
#include "syscall.h"
#include <errno.h>

int __futex(volatile int *addr, int op, int val, void *ts)
{
#warning "__futex not supported"
	//OLD
	//return syscall(SYS_futex, addr, op, val, ts);
	//NEW
	return ENOTSUP;
}
