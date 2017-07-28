/*
 * Copyright (c) 2016 Virginia Tech,
 * Author: bielsk1@vt.edu
 * All rights reserved.
 */

#ifndef _LIBBOMP_H
#define	_LIBBOMP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "spin.h"

#ifdef _DEBUG_POOL
# define DEBUGPOOL( ... ) fprintf(stderr, __VA_ARGS__)
#else
# define DEBUGPOOL( ... )
#endif

struct bomp_work {
    void (*fn)(void *);
    void *data;
    unsigned thread_id;
    struct bomp_barrier *barrier;
};

struct bomp_thread_local_data {
    void *thr; // thread reference 
    struct bomp_work *work;
};

extern volatile unsigned g_thread_numbers;
extern unsigned bomp_num_threads;
extern bool bomp_dynamic_behaviour;
extern bool bomp_nested_behaviour;
extern struct bomp_thread_local_data **g_array_thread_local_data;

void parallel_init(void);
int bomp_thread_fn(void *xdata);
void bomp_start_processing(void (*fn) (void *), void *data, unsigned nthreads);
void bomp_end_processing(void);

/* These functions are called from GCC-generated code */
void GOMP_ordered_start(void);
void GOMP_ordered_end(void);
bool GOMP_single_start(void);
void GOMP_barrier(void);
void GOMP_atomic_start(void);
void GOMP_atomic_end(void);
void GOMP_parallel_start(void (*) (void *), void *, unsigned);
void GOMP_parallel_end(void);
void GOMP_critical_start(void);
void GOMP_critical_end(void);

#endif	/* _LIBBOMP_H */
