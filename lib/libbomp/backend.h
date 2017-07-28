/*
 * Copyright (c) 2016 Virginia Tech,
 * Author: bielsk1@vt.edu
 * All rights reserved.
 */

#ifndef BOMP_BACKEND_H
#define BOMP_BACKEND_H

typedef int (*bomp_thread_func_t)(void *);

void backend_set_numa(unsigned id);
void backend_run_func_on(int core_id, void* cfunc, void *arg);
void* backend_get_tls(void);
void backend_set_tls(void *data);
void* backend_get_thread(void);
void backend_init(void);
void backend_exit(void);
void backend_thread_exit(void);
struct thread *backend_thread_create_varstack(bomp_thread_func_t start_func,
                                              void *arg, size_t stacksize);
#endif /* BOMP_BACKEND_H */
