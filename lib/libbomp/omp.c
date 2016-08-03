/*
 * Copyright (c) 2016 Virginia Tech,
 * Author: bielsk1@vt.edu
 * All rights reserved.
 */

#include <time.h>

#include "popcorn_threadpool.h"
#include "libbomp.h"
#include "omp.h"
#include "kmp.h"
#include "spin.h"

unsigned bomp_num_threads =1;
bool bomp_dynamic_behaviour = false;
bool bomp_nested_behaviour = false;

void __attribute__((constructor))
bomp_custom_init(void)
{
    parallel_init();
    backend_init();
}

void __attribute__((destructor))
bomp_custom_exit(void)
{
    backend_exit();
}

void omp_kill_thread(int pop, int corn, void* args){
  int newThreadNum = args;
  int me_id = omp_get_thread_num();
  DEBUGPOOL("##TH:%d####entered %s NEWNum:%d.\n",omp_get_thread_num(),__func__,newThreadNum);
  if( omp_get_thread_num() < newThreadNum ){
        DEBUGPOOL(">>%s: Thread Legal %d New Max %d\n",__func__,omp_get_thread_num(),newThreadNum);
        return;
  }else{
        DEBUGPOOL(">>>%s: new NumThreads:%d . Terminating myself! ID:%d\n",__func__,newThreadNum,omp_get_thread_num());
        int ret;
        pthread_exit(&ret);
  }
}//end omp_kill_thread

void omp_set_num_threads(int num_threads){
  int numThreadsNow = omp_get_num_threads();
  DEBUGPOOL("Currently %d threads exist in pool. New num requested: %d\n",numThreadsNow,num_threads);

  //Add more threads to "pool"
  if(numThreadsNow < num_threads){
    //Figure out how many additional needed
    int diff = num_threads - numThreadsNow;
    int i;

    //Need to re-ALLOC pool->threads
    pool->threads = realloc(pool->threads, sizeof(pthread_t) * num_threads);

    for(i = 0 ; i < diff ; i++){
        perThread_info* my_data = malloc(sizeof(perThread_info));
        my_data->pool = pool;
        my_data->thread_id = numThreadsNow + i;
        DEBUGPOOL("%s: loop %d new Thread_ID:%d\n",__func__,i,numThreadsNow+i);

        if(pthread_create(&(pool->threads[numThreadsNow+i-1]), NULL, threadpool_run, (void*)my_data) != 0){
          //something messed up
          DEBUGPOOL("%s: Creation of thread %d failed!\n",__func__,numThreadsNow+i);
	  threadpool_destroy(pool);
          return;
        }
        //success and increment counters
        DEBUGPOOL("%s: Started ADDITIONAL worker thread %d!\n", __func__, numThreadsNow + i );
        pool->thread_count++;
        pool->started++;
    }//end for
        //Update Global bomp_num_threads
	bomp_barrier_updateMax(pool->global_barrier,num_threads);
        bomp_num_threads = num_threads;
        return;
  }

  //Essentially get rid of "threadpool" (only Master remains)
  if( num_threads == 1){
    //Update Global bomp_num_threads
    bomp_barrier_updateMax(pool->global_barrier,num_threads);
    bomp_num_threads = num_threads;
    return;
  }

  //Less number of threads now
  if(numThreadsNow > num_threads){
    //Figure out how many need to be shutdown
    int diff = numThreadsNow - num_threads;
    int i,res;

    //Update Global bomp_num_threads
    bomp_barrier_updateMax(pool->global_barrier,num_threads);
    bomp_num_threads = num_threads;
    DEBUGPOOL("Less NOW bomp_num_threads:%d\n",bomp_num_threads);
    return;
  }//end else if

  //No Changes
  if(num_threads == numThreadsNow){
    DEBUGPOOL("<<<<<<< No Changes to Num Threads >>>>>>>\n");
    return;
  }

  //INVALID 
  if(num_threads < 0){
    DEBUGPOOL("%s: Please use number greater than 0! Input:%d\n",__func__,num_threads);
    DEBUGPOOL("Num Threads being used: %d\n",numThreadsNow);
    return;
  }
}//END omp_set_num_threads

int omp_get_num_threads(void)
{
  return bomp_num_threads;
}

int omp_get_max_threads(void)
{
  return bomp_num_threads;
}

int omp_get_thread_num(void)
{
    if (g_thread_numbers == 1) {
        return 0;
    }

    struct bomp_thread_local_data *tls = backend_get_tls();
    return tls->work->thread_id;
}

int omp_get_num_procs(void)
{
    return 1;
}

void omp_set_dynamic(int dynamic_threads)
{
    if ( dynamic_threads == 0 ) {
        bomp_dynamic_behaviour  = false;
    }
    else {
        bomp_dynamic_behaviour  = true;
    }
}

int omp_get_dynamic(void)
{
    return bomp_dynamic_behaviour;
}

int omp_in_parallel(void)
{
    if(g_thread_numbers == 1) {
        return 0;
    }
    return g_thread_numbers;
}

void omp_set_nested(int nested)
{
    if (nested == 0) {
        bomp_nested_behaviour = false;
    } else {
        bomp_dynamic_behaviour = true;
    }
}

int omp_get_nested(void)
{
    return bomp_dynamic_behaviour;
}

double omp_get_wtime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}
