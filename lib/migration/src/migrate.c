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
    trap();
}

