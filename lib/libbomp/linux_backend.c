/*
 * Copyright (c) 2016 Virginia Tech,
 * Author: bielsk1@vt.edu
 * All rights reserved.
 */

#ifndef NUMA
# ifndef _GNU_SOURCE
# define _GNU_SOURCE
# endif
#include <sched.h>
#endif


#include <pthread.h>
#ifdef NUMA
#include <numa.h>
#endif

#include <stdio.h>
#include <assert.h>

#include "backend.h"
#include "libbomp.h"
#include "omp.h"
#include "popcorn_threadpool.h"

//GLOBAL POOL VAR
threadpool_t *pool = NULL;

void backend_set_numa(unsigned id)
{
#if 0
#ifdef NUMA
    struct bitmask *bm = numa_allocate_cpumask();
    numa_bitmask_setbit(bm, id);
    numa_sched_setaffinity(0, bm);
    numa_free_cpumask(bm);
#else
    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(id, &cpu_mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu_mask);
#endif
#endif
}

void backend_run_func_on(int core_id, void* cfunc, void *arg){
	/* NOP */
}

static pthread_key_t pthread_key;

void *backend_get_tls(void)
{
    return pthread_getspecific(pthread_key);
}
 
void backend_set_tls(void *data)
{
    assert(data);
    pthread_setspecific(pthread_key, data);
}

void *backend_get_thread(void)
{
    return (void *)pthread_self();
}

static int remote_init(void *dumm)
{
    return 0;
}

void backend_span_domain_default(int nos_threads)
{
    /* nop */
}

void backend_span_domain(int nos_threads, size_t stack_size)
{
    /* nop */
}

void backend_init(void)
{
  //create key for app
  int r = pthread_key_create(&pthread_key, NULL);
  if (r != 0) {
    printf("pthread_key_create failed\n");
  }
  DEBUGPOOL("%s: key_create success!\n",__func__);

  char s[512];
  FILE *fd;
  int num_cores= 0;

  fd = fopen("/proc/cpuinfo", "r");
  if (fd == NULL) {
    printf("ALERT: /proc/cpuinfo could not be read. DEFAULT being used (%d)\n",bomp_num_threads);
    num_cores = 1;
  } else {
    while (fgets(s, 512, fd) != NULL) {
      if (strstr(s, "GenuineIntel") != NULL || strstr(s, "AuthenticAMD") != NULL){
        num_cores++;
      }
    }
    int res = fclose(fd);
    DEBUGPOOL("FCLOSE: result %d\n",res);
  }

  /* Initialize Threadpool HERE! */
  pool = threadpool_create(num_cores-1,1024);
  DEBUGPOOL("%s: Threadpool Initiated, %d cores detected\n",__func__,num_cores);    
}//END backend_init

void backend_exit(void)
{
  int res = threadpool_destroy(pool);
  if(res == 0){
    DEBUGPOOL("%s(): Success, Threadpool destroyed!\n",__func__);
  }else{
    printf("ERROR: %s| Threadpool Destroy Error %d\n",__func__,res);
  }
	/* nop */
#ifdef SHOW_PROFILING	
	dump_sched_self();
#endif	
}

void backend_create_time(int cores){
    /* nop */
}

void backend_thread_exit(void){ }

struct thread *backend_thread_create_varstack(bomp_thread_func_t start_func,
                                              void *arg, size_t stacksize)
{
    start_func(arg);
    return NULL;
}
