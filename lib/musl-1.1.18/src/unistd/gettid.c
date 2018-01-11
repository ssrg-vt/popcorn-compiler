#include <unistd.h>
#include "syscall.h"

pid_t gettid(void)
{
	return __syscall(SYS_gettid);
}
