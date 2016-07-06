/**
 * \file
 * \brief Implementation of backend functions on Linux
 */

// Popcorn version
// Copyright Antonio Barbalace, SSRG, VT, 2013
// current version with ZERO futexes

/*
 * Copyright (c) 2007, 2008, 2009, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
//#include <numa.h>
#include <sched.h>
//#include <pthread.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <asm/ldt.h>

#include "backend.h"
#include "omp.h"

#include "cthread.h"

static unsigned long saved_selector = -1;

void backend_set_numa(unsigned id)
{
    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    CPU_SET(id, &cpu_mask);

    cthread_setaffinity_np(0, sizeof(cpu_set_t), &cpu_mask);
}

void backend_run_func_on(int core_id, void* cfunc, void *arg)
{
	cthread_t tid;
    int r = cthread_create(&tid, (void*)((long)core_id), cfunc, arg);
    if (r == -1) {
        printf("%s: [main %ld] cthread_create %d FAILED\n", __func__, (long)getpid(), r);
        exit (1234);
    }
}

static cthread_key_t backend_key = -1;

void *backend_get_tls(void)
{
    return cthread_getspecific(backend_key);
}

void backend_set_tls(void *data)
{
    cthread_setspecific(backend_key, data);
}

void *backend_get_thread(void)
{
    return (void *) cthread_self();
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

static int init_calls=0;

void backend_init(void)
{
  // check if the function was called before ----------------------------------
  printf("%s: [main %ld] init_call %d\n", __func__, (long) getpid(), ++init_calls);
  if (init_calls > 1)
    exit(1234);

  // alloca pthread keys ------------------------------------------------------
  int r = cthread_key_create(&backend_key, NULL);
  if (r != 0) {
      printf("%s: cthread_key_create %d FAILED\n", __func__, r);
      exit(1234);
  }
//  printf("%s: cthread_key_create %d\n", __func__, r);

  saved_selector = cthread_initialize();
}

void backend_exit (void)
{
	// TODO liberate the memory allocated --- qui o nel previous context (each on the specific context!)

// PUT this in cthread as a debugging aid
//we have to decide about the memory stuff ...
/*  int _ret;
  FILE * fp = fopen("malloc.info", "w");
  if (fp) {
    _ret = malloc_info( 0, fp);
    fclose(fp);
    //printf("malloc info ret %d\n", _ret);
  }
  else
    perror ("failed to open file\n");
*/

  cthread_restore(saved_selector);
  //NOTE getpid, despite we are not using nptl it is accessing the pid field in the TLS (nptl specific)
  printf("%s: [main %ld] exit_call\n", __func__, (long)getpid());

#ifdef SHOW_PROFILING
  dump_sched_self ();
#endif  
}

void backend_create_time(int cores)
{
    /* nop */
}

//NOTE --- the following two are together

void backend_thread_exit(void)
{
    /* nop */
}

struct thread *backend_thread_create_varstack(bomp_thread_func_t start_func,
                                              void *arg, size_t stacksize)
{
    start_func(arg);
    return NULL;
}
