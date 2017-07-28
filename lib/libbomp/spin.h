/*
 * Copyright (c) 2016 Virginia Tech,
 * Author: bielsk1@vt.edu
 * All rights reserved.
 * File Desc: Spin Synchronization for mutexs and barriers
 */

#ifndef BOMP_SPIN_H
#define BOMP_SPIN_H

#include "omp.h"

/** \brief spinlock */
// why 64-bit? this means we need an extra prefix on the lock ops... -AB
typedef volatile unsigned long bomp_lock_t;
uint64_t stuck[64];

static inline void bomp_lock(bomp_lock_t *lock)
{
// taken from https://lkml.org/lkml/2012/7/6/568
#ifdef __aarch64__
	unsigned int tmp;
    __asm__ __volatile__(
	"	sevl\n"
	"1:	wfe\n"
	"2:	ldaxr	%w0, [%1]\n"
	"	cbnz	%w0, 1b\n"
	"	stxr	%w0, %w2, [%1]\n"
	"	cbnz	%w0, 2b\n"
	: "=&r" (tmp)
	: "r" (lock), "r" (1)
	: "memory");
#elif __x86_64__
    __asm__ __volatile__("0:\n\t"
                         "cmpq $0, %0\n\t"
                         "je 1f\n\t"
                         "pause\n\t"
                         "jmp 0b\n\t"
                         "1:\n\t"
                         "lock btsq $0, %0\n\t"
                         "jc 0b\n\t"
                         : "+m" (*lock) : : "memory", "cc");
#else
    #error "Architecture not supported"
#endif
}

static inline void bomp_unlock(bomp_lock_t *lock)
{
    *lock = 0;
}

static inline void bomp_lock_init(bomp_lock_t *lock)
{
    /* nop */
}

struct bomp_barrier {
    unsigned max;
    volatile unsigned cycle;
    volatile unsigned counter;
};

static inline void bomp_barrier_init(struct bomp_barrier *barrier , int count)
{
    barrier->max     = count;
    barrier->cycle   = 0;
    barrier->counter = 0;
}

static inline void bomp_clear_barrier(struct bomp_barrier *barrier)
{
    /* nop */
}

static inline void bomp_barrier_updateMax(struct bomp_barrier *barrier, int new_max)
{
    barrier->max = new_max;
    DEBUGPOOL("%s: NewMax # Threads:%d\n",__func__,barrier->max);
}

static inline void bomp_barrier_wait(struct bomp_barrier *barrier)
{
    int cycle = barrier->cycle;
    if (__sync_fetch_and_add(&barrier->counter, 1) == barrier->max - 1) {
        barrier->counter = 0;
        barrier->cycle = !barrier->cycle;
    } else {
        uint64_t waitcnt = 0;

        while (cycle == barrier->cycle) {
            waitcnt++;
        }

        if(waitcnt > 10000000) {
            stuck[omp_get_thread_num()]++;
            /* char buf[128]; */
            /* sprintf(buf, "thread %d stuck in barrier\n", */
            /*         omp_get_thread_num()); */
            /* sys_print(buf, strlen(buf)); */
        }
    }                                       
}//END bomp_barrier_wait

#endif /* BOMP_SPIN_H */
