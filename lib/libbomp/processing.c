/*
 * Copyright (c) 2016 Virginia Tech,
 * Author: bielsk1@vt.edu
 * All rights reserved.
 */

#include "libbomp.h"
#include "backend.h"
#include "linux_backend.c"
#include "kmp.h"

static int count = 0;
volatile unsigned g_thread_numbers = 1;
struct bomp_thread_local_data **g_array_thread_local_data;

#define THREAD_OFFSET   0
/* #define THREAD_OFFSET   12 */

void bomp_set_tls(void *xdata)
{
    struct bomp_thread_local_data *local;
    struct bomp_work *work_data = (struct bomp_work*)xdata;

    /* Populate the Thread Local Storage data */
    local = calloc(1, sizeof(struct bomp_thread_local_data));
    assert(local != NULL);
    local->thr = backend_get_thread();
    local->work = work_data;
    g_array_thread_local_data[work_data->thread_id] = local;
    backend_set_tls(local);
}

int bomp_thread_fn(void *xdata)
{
    struct bomp_work *work_data = xdata;

    DEBUGPOOL("%s: start thread:%d\n",__func__,work_data->thread_id);
    backend_set_numa(work_data->thread_id);
    DEBUGPOOL("bomp_thread_fn: 0x%lx\n",work_data->fn);

    bomp_set_tls(work_data);
    work_data->fn(work_data->data);

    DEBUGPOOL("tid %d, finished work, entering barrier\n", omp_get_thread_num());
    /* Wait for the Barrier */
    bomp_barrier_wait(work_data->barrier);
    return 0;
}

void bomp_start_processing(void (*fn) (void *), void *data, unsigned nthreads){
    unsigned i;
    g_thread_numbers = nthreads;

    /* Alllocate and initialize TLS!>!? (from bomp structures) */
    struct bomp_work *generic_xdata, *xdata;
//Data size should be the same for parallel regions
//we are just passing pointers to omp regions and related omp args to the threads
//    struct bomp_barrier *generic_barrier; 
//do I have to make a new barrier for each parallel region?? I think so. Maybe.

    char *memory = calloc(1, nthreads * sizeof(struct bomp_thread_local_data *)
                            + nthreads * sizeof(struct bomp_work));
    assert(memory != NULL);

    g_array_thread_local_data = (struct bomp_thread_local_data **)memory;
    memory += nthreads * sizeof(struct bomp_thread_local_data *);

    /* For main thread */
    xdata = (struct bomp_work * )memory;
    memory += sizeof(struct bomp_work);

    xdata->fn = fn;
    xdata->data = data;
    xdata->thread_id = 0;
    xdata->barrier = pool->global_barrier;

    //setting up master threads TLS
    bomp_set_tls(xdata);
  
    for(i= 1 ; i < nthreads; i++){
	xdata = (struct bomp_work *)memory;
        memory += sizeof(struct bomp_work);

        xdata->fn = fn;
        xdata->data = data;
        xdata->thread_id = i;
        xdata->barrier = pool->global_barrier;

	    DEBUGPOOL("%s: Adding task %p\n",__func__,bomp_thread_fn);
	    threadpool_add( pool, bomp_thread_fn, xdata );
	    DEBUGPOOL("%s: !!Success %d Adding task %p\n",__func__,i,bomp_thread_fn);
    }
}

void bomp_end_processing(void)
{
    /* Cleaning of thread_local and work data structures */
    int i = 0;
    count++;

    bomp_barrier_wait(g_array_thread_local_data[i]->work->barrier);

    //  Clear the barrier created
    bomp_clear_barrier(g_array_thread_local_data[i]->work->barrier);

    // XXX: free(g_array_thread_local_data); why not? -AB
    g_array_thread_local_data = NULL;
    g_thread_numbers = 1;
   
    DEBUGPOOL("returning from:%s, tID:%d\n", __func__,  omp_get_thread_num());
}
