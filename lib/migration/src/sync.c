/*
 * sync.c
 * Copyright (C) 2018 Ho-Ren(Jack) Chuang <horenc@vt.edu>
 *
 * Distributed under terms of the MIT license.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* for syscall*/
#include "config.h" /* for SYS_* */
//#include <sys/syscall.h>
//#include <stdint.h>
//#include <string.h>
//#include <signal.h>
//#include <assert.h>

int popcorn_tso_begin()
{
    //printf("popcorn_tso_begin: user\n");
    return syscall(SYS_popcorn_tso_begin, 0, NULL);
}

int popcorn_tso_fence()
{
    //printf("popcorn_tso_fence: user\n");
    return syscall(SYS_popcorn_tso_fence, 0, NULL);
}

int popcorn_tso_end()
{
    //printf("popcorn_tso_end: user\n");
    return syscall(SYS_popcorn_tso_end, 0, NULL);
}


int popcorn_tso_begin_manual()
{
    //printf("popcorn_tso_begin_manual: user\n");
    return syscall(SYS_popcorn_tso_begin_manual, 0, NULL);
}

int popcorn_tso_fence_manual()
{
    //printf("popcorn_tso_fence_manual: user\n");
    return syscall(SYS_popcorn_tso_fence_manual, 0, NULL);
}

int popcorn_tso_end_manual()
{
    //printf("popcorn_tso_end_manual: user\n");
    return syscall(SYS_popcorn_tso_end_manual, 0, NULL);
}
