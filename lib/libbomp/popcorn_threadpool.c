/*
 * POPCORN Threadpool Implementation File
 * Author: bielsk1@vt.edu
 * Date: 07/11/16
 * Version: 0.1
 * Copyright (c) 2016 VT
 */
#include "popcorn_threadpool.h"
#include "omp.h"
#include "libbomp.h"

/*
 * num_threads: Number of threads that should be in the the queue
 * queue_size: Max size of queue?
 */
threadpool_t *threadpool_create( int num_threads, int queue_size){
	//Check if app killed all queue threads?
	if(num_threads <= 0 || queue_size <= 0 ){
		return NULL;
	}

	threadpool_t *pool;
	int i;
	bomp_num_threads = num_threads+1; //1 for master thread
	
	if((pool = (threadpool_t *) malloc(sizeof(threadpool_t))) == NULL){
	  if(pool){
	    threadpool_free(pool);
	  }
	  return NULL;
	}

	/*Initialize threadpool struct components*/
	pool->thread_count = 0;
	pool->queue_size = queue_size;
	pool->head = pool->tail = pool->count = 0;
	pool->shutdown = pool->started = 0;
	
	/*Allocate the children threads and the task queue*/
	pool->threads = (pthread_t *) malloc(sizeof(pthread_t) * num_threads);
	pool->queue = (threadpool_task_t *) malloc(sizeof(threadpool_task_t) * queue_size);

	/*Allocate Global Barrier*/
	struct bomp_barrier* global_barrier = calloc(1, sizeof *global_barrier);
	bomp_barrier_init(global_barrier, num_threads);
	pool->global_barrier = global_barrier;
		
	/*Initialize Queue lock & notification variable*/
	if( (pthread_mutex_init(&(pool->lock), NULL) != 0) ||
		(pthread_cond_init(&(pool->notify), NULL) != 0) ||
		(pool->threads == NULL) ||
		(pool->queue == NULL)
	){
		if(pool){
			threadpool_free(pool);
		}
		return NULL;
	}//end if

	/*Start the worker threads (can subtract one since master thread already exists)*/
	for(i = 0; i < num_threads ; i++){
	  perThread_info * my_data = malloc(sizeof(perThread_info));
	  my_data->pool = pool;
	  my_data->thread_id = i+1;

	  if(pthread_create(&(pool->threads[i]), NULL, threadpool_run, (void*)my_data) != 0){
	    //failed somewhere destroy and exit
	    printf("ALERT: SOMETHING FAILED in pthread_create\n");
	    threadpool_destroy(pool);
	    return NULL;
	  }//end if
	  DEBUGPOOL("Started worker thread %d!\n",my_data->thread_id);
	  //success and increment counters
	  pool->thread_count++;
	  pool->started++;
	}

	//return the newly created threadpool
	return pool;
}//END threadpool_create

/* Add a task to completed by the threadpool*/
int threadpool_add( threadpool_t *pool, void (*fn)(void *), void *args){
	int result = 0;
	int next;
	
	if(pool == NULL || fn == NULL){
		return threadpool_invalid;
	}

	/************* BEGIN LOCK ******************/
	if(pthread_mutex_lock(&(pool->lock)) != 0){
		return threadpool_lock_failure;
	}

	next = (pool->tail+1) % pool->queue_size;

	do {
	  if(pool->count == pool->queue_size){
		result = threadpool_queue_full;
		break;
	  }

	  if(pool->shutdown){
		result = threadpool_shutdown;
		break;
	  }
	  
	  /*Add the given task to the queue*/
	  pool->queue[pool->tail].fn = fn;
	  pool->queue[pool->tail].args = args;
	  DEBUGPOOL("%s: GGGGGGGGG FN:%p ARG:%p.\n",__func__, pool->queue[pool->tail].fn, pool->queue[pool->tail].args);
	  //pool->queue[pool->tail].barrier =(struct bomp_barrier*) args;
	  pool->count = pool->count + 1;

	  //NEW from BOMP
	  //pool->queue[pool->tail].data = (struct bomp_work*)bomp_work;
	  pool->tail = next;

	  /*pthread_cond_broadcast*/
	  /* TODO: Change this to broadcast???? */
	  //if(pthread_cond_signal(&(pool->notify)) != 0){
	  if(pthread_cond_broadcast(&(pool->notify)) != 0){
	    result = threadpool_lock_failure;	    break;
	  }
	} while(0);
	
	if(pthread_mutex_unlock(&(pool->lock)) != 0){
	  result = threadpool_lock_failure;
	}
	/************* END LOCK ***************************/
	
	DEBUGPOOL("%s: result %d\n",__func__,result);
	return result;
}//END threadpool_add

/* Destroy the threadpool */
int threadpool_destroy( threadpool_t *pool){
	int i, result= 0;
	if( pool == NULL){
	  return threadpool_invalid;
	}

	if(pthread_mutex_lock(&(pool->lock)) != 0){
	  return threadpool_lock_failure;
	}

	do{
	  /*Someone signaled Shutdown already*/
	  if(pool->shutdown){
	    //result = threadpool_shutdown;
	    break;
	  }

	  pool->shutdown = 1;

	  /*Wake up all work threads*/
	  if((pthread_cond_broadcast(&(pool->notify)) != 0) || 
		(pthread_mutex_unlock(&(pool->lock)) != 0))  {
	    result = threadpool_lock_failure;
	  }
	
	  /*Join All Worker Threads */
	  for(i = 0 ; i < pool->thread_count ; i++){
	    if(pthread_join(pool->threads[i], NULL) != 0){
		result = threadpool_thread_failure;
	    }
	  }
	} while(0);
	pool->thread_count = 0;

	/* Deallocate Threadpool */
	if(!result){
	  threadpool_free(pool);
	}
	return result;
}//END threadpool_destroy

int threadpool_free(threadpool_t *pool){
	if(pool == NULL || pool->started > 0){
	  return -1;
	}
	/* Did threadpool_create succeed? */
	if(pool->threads){
	  free(pool->threads);
	  free(pool->queue);
	  free(pool->global_barrier);
	
	  pthread_mutex_lock(&(pool->lock));
	  pthread_mutex_destroy(&(pool->lock));
	  pthread_cond_destroy(&(pool->notify));
	}
	free(pool);
	return 0;
}//END threadpool_free

void* threadpool_run(void* threadpool){
	perThread_info * perThread_info_info = (perThread_info *) threadpool;
	threadpool_task_t task;
	// get POOL && Identify yourself (your thread ID)
	threadpool_t *pool = perThread_info_info->pool;
	unsigned thread_id = perThread_info_info->thread_id;

	while(1){
	  /* Attempt to Lock */
DEBUGPOOL(">>>ID:%d waiting for mutex\n", thread_id);
	  pthread_mutex_lock(&(pool->lock));
DEBUGPOOL(">>>ID:%d waiting for cond_wait\n", thread_id);
	
	  while((pool->count == 0) && (!pool->shutdown)){
	    //put thread to sleep
	    pthread_cond_wait(&(pool->notify), &(pool->lock));
	  }

	  if((pool->shutdown == graceful_shutdown) && (pool->count == 0)){
	    DEBUGPOOL(">>>ID:%d will die now\n", thread_id);
	    break;
	  }

	  /* Grab a task from FRONT of queue*/
	  task.fn = pool->queue[pool->head].fn;
	  task.args = pool->queue[pool->head].args;
	  pool->head = (pool->head +1) % pool->queue_size;
	  pool->count = pool->count - 1;
	  /* Unlock */
	  pthread_mutex_unlock(&(pool->lock));

	  //NEW FROM BOMP
	  //struct bomp_work *generic_work_data = task.args;
	  //generic_work_data->thread_id = thread_id;
	  //bomp_set_tls(generic_work_data);
	  bomp_set_tls((struct bomp_work*) task.args);

	  /* Start Task */
	  DEBUGPOOL(">ID:%d NEw TASK thread:%d, fn:0x%lx\n",thread_id,omp_get_thread_num(),task.fn);
	  (*(task.fn))(task.args);
	}//END Inifinite loop

	pool->started--;
	pthread_mutex_unlock(&(pool->lock));
	pthread_exit(NULL);
}//END threadpool_run

int threadpool_getNumThreads(threadpool_t *pool){
  return pool->thread_count;
}//END threadpool_getNumThreads
