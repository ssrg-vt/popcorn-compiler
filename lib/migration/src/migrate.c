#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "migrate.h"

volatile int __indicator = -1;


/* Check if we should migrate, trap if migration needed. */
void check_migrate(void (*callback)(void *), void *callback_data)
{
    if(__indicator < 0)
        return;
#if defined(__x86_64__)
    __asm__ volatile("int $0x03");
#elif defined(__aarch64__)
    __asm__ volatile(".inst 0xd4200000");
#endif
}

