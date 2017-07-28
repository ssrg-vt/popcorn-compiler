/*
 * Copyright (c) 2016 Virginia Tech,
 * Author: bielsk1@vt.edu
 * All rights reserved.
 */

#include "popcorn_threadpool.h"
#include "libbomp.h"
#include "omp.h"

static volatile int nested = 0;
static bomp_lock_t critical_lock;
static bomp_lock_t atomic_lock;

void GOMP_critical_start(void)
{
  bomp_lock(&critical_lock);
}

void GOMP_critical_end(void)
{
  bomp_unlock(&critical_lock);
}

void GOMP_ordered_start(void)
{
    /* nop */
}

void GOMP_ordered_end(void)
{
    /* nop */
}

void GOMP_parallel_start(void (*fn) (void *), void *data, unsigned nthreads)
{
    /* Identify the number of threads that can be spawned and start the processing */
    if (!omp_in_parallel()) {
        if (nthreads == 0
            || (bomp_dynamic_behaviour && bomp_num_threads < nthreads)) {
            nthreads = bomp_num_threads;
        }
	DEBUGPOOL("~~~~%s: about to start bomp_start_processing\n",__func__);
        bomp_start_processing(fn, data, nthreads);
    }
    __sync_fetch_and_add(&nested, 1);
    DEBUGPOOL(">OPenMP nested Lvl:%d\n",nested);
}

void GOMP_parallel_end(void)
{
    if(nested == 1) {
        bomp_end_processing();
    }
    __sync_fetch_and_sub(&nested, 1);
    DEBUGPOOL(">eOPenMP nested Lvl:%d\n",nested);
}

/* This function should return true for just the first thread */
bool GOMP_single_start(void)
{
    struct bomp_thread_local_data *local = backend_get_tls();
    if(local == NULL || local->work->thread_id == 0){
        return true;
    }
    return false;
}

void GOMP_barrier(void)
{
    struct bomp_thread_local_data *th_local_data = backend_get_tls();
    assert(th_local_data != NULL);
    bomp_barrier_wait(th_local_data->work->barrier);
}

void GOMP_atomic_start (void)
{
   bomp_lock(&atomic_lock);
}

void GOMP_atomic_end (void)
{
   bomp_unlock(&atomic_lock);
}

void parallel_init(void)
{
   bomp_lock_init(&atomic_lock); // (NOP right now)
   bomp_lock_init(&critical_lock); // (NOP right now)
}
