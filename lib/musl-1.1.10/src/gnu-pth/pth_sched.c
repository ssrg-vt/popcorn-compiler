/*
**  GNU Pth - The GNU Portable Threads
**  Copyright (c) 1999-2006 Ralf S. Engelschall <rse@engelschall.com>
**
**  This file is part of GNU Pth, a non-preemptive thread scheduling
**  library which can be found at http://www.gnu.org/software/pth/.
**
**  This library is free software; you can redistribute it and/or
**  modify it under the terms of the GNU Lesser General Public
**  License as published by the Free Software Foundation; either
**  version 2.1 of the License, or (at your option) any later version.
**
**  This library is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**  Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License along with this library; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
**  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
**
**  LSCHED->pth_sched.c: Pth thread scheduler, the real heart of Pth
*/
                             /* ``Recursive, adj.;
                                  see Recursive.''
                                     -- Unknown   */
#include "pth_p.h"

#if cpp
	/* Scheduler */
//typedef struct timeval pth_time_t;
struct scheduler_s
{
    int id;
	/* public scheduler variables*/
	/* can be modified only by the blancer_id */
	volatile char  	new;	     	/* Bool value */
	volatile char	stop;		/* Bool Value */
	volatile int 	pth_receivepipe[2];   	/* queue of new threads                  */
	/* can be read by the blancer_id */
	volatile float 	pth_loadval;    /* average scheduler load value          */

	/* private scheduler varibles */
	int 	     nb_threads;     /* Number of received threads  */
	pth_t        pth_main;       /* the main thread                       */
	pth_t        pth_sched;      /* the permanent scheduler thread        */
	pth_t        pth_current;    /* the currently running thread          */
	pth_pqueue_t pth_NQ;         /* queue of new threads                  */
	pth_pqueue_t pth_RQ;         /* queue of threads ready to run         */
	pth_pqueue_t pth_WQ;         /* queue of threads waiting for an event */
	pth_pqueue_t pth_SQ;         /* queue of suspended threads            */
	pth_pqueue_t pth_DQ;         /* queue of terminated threads           */
	int          pth_favournew;  /* favour new threads on startup         */

	pth_time_t   pth_loadticknext;
	pth_time_t   pth_loadtickgap;

	/* Shouldn't the signals be shared across schedulers ? */
	int          pth_sigpipe[2]; /* internal signal occurrence pipe       */
	sigset_t     pth_sigpending; /* mask of pending signals               */
	sigset_t     pth_sigblock;   /* mask of signals we block in scheduler */
	sigset_t     pth_sigcatch;   /* mask of signals we have to catch      */
	sigset_t     pth_sigraised;  /* mask of raised signals                */

	pthread_t    pthread;     /* For all threads except the main one   */
};


#define LSCHED get_local_scheduler()
#define LSCHED_ID get_local_scheduler()->id
#define pth_current (LSCHED->pth_current)


#endif

intern volatile int total_nb_threads;		/* init 0 */
intern int          pth_joinpipe[2]; 		/* dead thread occurrence pipe       */

#define MAX_SCHEDULER 96
static int nb_nodes;
static int nb_schedulers;
static int nb_schedulers_requested;
static struct scheduler_s schedulers[MAX_SCHEDULER];

static int next_id;
static int scheduler_ids[MAX_SCHEDULER];
static __thread int scheduler_id;

static volatile int balancer_id = 0;

static int sched_share=0;

intern struct scheduler_s* get_local_scheduler()
{
	return &schedulers[scheduler_id];
}

static void update_scheduler_id(int id)
{
    scheduler_ids[id] = id;
    scheduler_id = id;
    schedulers[id].id = id;
}

static int init_receive_pipe(struct scheduler_s *sched)
{
	//initialise the receiveing queue
	//pth_pqueue_init(&schedulers[0].pth_receive);
    if (pipe(sched->pth_receivepipe) == -1)
        return pth_error(FALSE, errno);
	if(fcntl(sched->pth_receivepipe[0], F_SETFL, O_NONBLOCK))
        	return pth_error(FALSE, errno);
	if(fcntl(sched->pth_receivepipe[1], F_SETFL, O_NONBLOCK))
        	return pth_error(FALSE, errno);

}

/* initialize the scheduler ingredients */
static int pth_scheduler_init_id(int id)
{
    struct scheduler_s* sched;

    sched = &schedulers[id];

    update_scheduler_id(id);

    /* create the internal signal pipe */
    if (pipe(LSCHED->pth_sigpipe) == -1)
        return pth_error(FALSE, errno);
    if (pth_fdmode(LSCHED->pth_sigpipe[0], PTH_FDMODE_NONBLOCK) == PTH_FDMODE_ERROR)
        return pth_error(FALSE, errno);
    if (pth_fdmode(LSCHED->pth_sigpipe[1], PTH_FDMODE_NONBLOCK) == PTH_FDMODE_ERROR)
        return pth_error(FALSE, errno);

    /* initialize the essential threads */
    sched->pth_sched   = NULL;
    pth_current = NULL;

    /* initalize the thread queues */
    pth_pqueue_init(&sched->pth_NQ);
    pth_pqueue_init(&sched->pth_RQ);
    pth_pqueue_init(&sched->pth_WQ);
    pth_pqueue_init(&sched->pth_SQ);
    pth_pqueue_init(&sched->pth_DQ);

    /* initialize scheduling hints */
    sched->pth_favournew = 1; /* the default is the original behaviour */

    /* initialize number of threads */
    LSCHED->nb_threads = 0;

    /* initialize load support */
    sched->pth_loadval = 1.0;
    LSCHED->pth_loadtickgap = (pth_time_t)PTH_TIME(1,0);
    pth_time_set(&LSCHED->pth_loadticknext, PTH_TIME_NOW);

    return TRUE;
}

/* initialize the scheduler ingredients */
intern int pth_scheduler_init(void)
{
	int i;
	char *thds_char;

	nb_schedulers = 1;

	pth_debug1("Initialising gnu pth scheduler\n");

	for(i=0; i<MAX_SCHEDULER; i++)
		scheduler_ids[i]=-1;

	/* For now we use a env. variable, later the number	*
	 * will be adapted  dynamically.			*/
	thds_char = getenv("GNU_PTH_THREADS");
	if(thds_char)
		nb_schedulers_requested = atoi(thds_char);
	else
		nb_schedulers_requested = nb_schedulers;
	pth_debug3("%s: number of schedulers requested = %d\n", __func__,
						nb_schedulers_requested);

	thds_char = getenv("GNU_PTH_NB_NODES");
	if(thds_char)
		nb_nodes = atoi(thds_char);
	else
		nb_nodes = -1;
	pth_debug3("%s: number of schedulers requested = %d\n", __func__,
						nb_schedulers_requested);

    if (pipe(pth_joinpipe) == -1)
        return pth_error(FALSE, errno);
	if(fcntl(pth_joinpipe[0], F_SETFL, O_NONBLOCK))
        	return pth_error(FALSE, errno);
	if(fcntl(pth_joinpipe[1], F_SETFL, O_NONBLOCK))
        	return pth_error(FALSE, errno);

	/* initialise the first scheduler */
    init_receive_pipe(&schedulers[0]);
	return pth_scheduler_init_id(0);
}

/* drop all threads (except for the currently active one) */
//FIXME: need to drop all schedulers not just the local one
intern void pth_scheduler_drop(void)
{
    pth_t t;

    /* clear the new queue */
    while ((t = pth_pqueue_delmax(&LSCHED->pth_NQ)) != NULL)
        pth_tcb_free(t);
    pth_pqueue_init(&LSCHED->pth_NQ);

    /* clear the ready queue */
    while ((t = pth_pqueue_delmax(&LSCHED->pth_RQ)) != NULL)
        pth_tcb_free(t);
    pth_pqueue_init(&LSCHED->pth_RQ);

    /* clear the waiting queue */
    while ((t = pth_pqueue_delmax(&LSCHED->pth_WQ)) != NULL)
        pth_tcb_free(t);
    pth_pqueue_init(&LSCHED->pth_WQ);

    /* clear the suspend queue */
    while ((t = pth_pqueue_delmax(&LSCHED->pth_SQ)) != NULL)
        pth_tcb_free(t);
    pth_pqueue_init(&LSCHED->pth_SQ);

    /* clear the dead queue */
    while ((t = pth_pqueue_delmax(&LSCHED->pth_DQ)) != NULL)
        pth_tcb_free(t);
    pth_pqueue_init(&LSCHED->pth_DQ);

    /*TODO:receive? */

    return;
}

/* kill the scheduler ingredients */
intern void pth_scheduler_kill(void)
{
    /* drop all threads */
    pth_scheduler_drop();

    if(LSCHED->nb_threads != 1)
    	pth_debug3("%s: Number of threads is %d\n", __func__, LSCHED->nb_threads);

    /* remove the internal signal pipe */
    close(LSCHED->pth_sigpipe[0]);
    close(LSCHED->pth_sigpipe[1]);
    close(pth_joinpipe[0]);
    close(pth_joinpipe[1]);
    return;
}

/*
 * Update the average scheduler load.
 *
 * This is called on every context switch, but we have to adjust the
 * average load value every second, only. If we're called more than
 * once per second we handle this by just calculating anything once
 * and then do NOPs until the next ticks is over. If the scheduler
 * waited for more than once second (or a thread CPU burst lasted for
 * more than once second) we simulate the missing calculations. That's
 * no problem because we can assume that the number of ready threads
 * then wasn't changed dramatically (or more context switched would have
 * been occurred and we would have been given more chances to operate).
 * The actual average load is calculated through an exponential average
 * formula.
 */
#define pth_scheduler_load(now) \
    if (pth_time_cmp((now), &LSCHED->pth_loadticknext) >= 0) { \
        pth_time_t ttmp; \
        int numready; \
        numready = pth_pqueue_elements(&LSCHED->pth_RQ); \
        pth_time_set(&ttmp, (now)); \
        do { \
            LSCHED->pth_loadval = (numready*0.25) + (LSCHED->pth_loadval*0.75); \
            pth_time_sub(&ttmp, &LSCHED->pth_loadtickgap); \
        } while (pth_time_cmp(&ttmp, &LSCHED->pth_loadticknext) >= 0); \
        pth_time_set(&LSCHED->pth_loadticknext, (now)); \
        pth_time_add(&LSCHED->pth_loadticknext, &LSCHED->pth_loadtickgap); \
    }

static void destroy_schedulers(int nb)
{
	//TODO
	//nb_schedulers--;
}

static int __get_new_thread_id(int start)
{
	int i;
	for(i=start; i<MAX_SCHEDULER; i++)
	{
		if(scheduler_ids[i]==-1)
			return i;
	}
	if(start == 0)
		return -1;
	return __get_new_thread_id(0);
}

static int get_new_thread_id()
{
	int ret = __get_new_thread_id(next_id);
	if(ret != -1)
	{
		next_id=ret+1;
		scheduler_ids[ret]= -2; // reserved
	}
	return ret;
}

void migrate(int nid, void (*callback)(void *), void *callback_data);
static void pth_scheduler_balance(void);
static void* new_scheduler_thread(void* arg)
{
    int rc;
	pth_t main;
    fd_set rfds;
    int fdmax = 0;
	int id = (int)(long)arg;
	pth_debug2("%s\n", __func__);

	/* initialize the scheduler */
	if(!pth_scheduler_init_id(id))
	{
        	pth_shield { pth_syscall_kill(); }
        	return pth_error(FALSE, EAGAIN);
    	}

	pth_init_always();

	//TODO: use passive waiting: using cond?
	while(!LSCHED->stop)
	{
		if(nb_nodes > id)
			migrate(id, NULL, NULL);

		/* if we already have threads use them. Otherwise if we are the
		 * balancer, check the receving queue. */
        FD_SET(LSCHED->pth_receivepipe[0], &rfds);
        if (fdmax < LSCHED->pth_receivepipe[0])
            fdmax = LSCHED->pth_receivepipe[0];

		if(LSCHED->nb_threads <= 1)
            while ((rc = pth_sc(select)(fdmax+1, &rfds, NULL, NULL, NULL)) < 0
                   && errno == EINTR) ;
			    ;//wait

		if(balancer_id == scheduler_id)
		{
			if(LSCHED->new)
				LSCHED->new=0;
		}

		/* Execute the scheduler. A yield will be better ? */
		pth_mctx_switch(&LSCHED->pth_main->mctx, &LSCHED->pth_sched->mctx);//will executer pth_scheduler_balance(); if...
	}

	return NULL;
}

int __pthread_create(pthread_t *restrict, const pthread_attr_t *restrict, void *(*)(void *), void *restrict);
static void create_schedulers(int nb)
{
	int i;
	int id;
	for(i=0; i<nb; i++)
	{
		id = get_new_thread_id();
		if(id==-1)
			return;

		/* few variable to communicate with */
		schedulers[id].new = 1;
		schedulers[id].pth_loadval = 0; //for load balancing
		//pth_pqueue_init(&schedulers[id].pth_receive);
        init_receive_pipe(&schedulers[id]);

		__pthread_create(&schedulers[id].pthread, NULL, new_scheduler_thread, (void*)(long)id);
		nb_schedulers++;
	}
}

static void update_schedulers(int wnb)
{

	pth_debug4("%s: # schedulers: current=%d, requested=%d\n", __func__,
					nb_schedulers, nb_schedulers_requested);

	if(wnb > nb_schedulers)
		create_schedulers(wnb-nb_schedulers);
	else
		destroy_schedulers(nb_schedulers - wnb);
}

static int __move_threads_queue(pth_t t, int boost)
{
	if(t->state == PTH_STATE_NEW || t->state == PTH_STATE_READY)
	{
		t->state=PTH_STATE_READY;
		if(boost)
			pth_pqueue_insert(&LSCHED->pth_RQ, pth_pqueue_favorite_prio(&LSCHED->pth_RQ), t);
        else
		    pth_pqueue_insert(&LSCHED->pth_RQ, t->prio, t);
	    pth_debug2("pth_scheduler: thread \"%s\" moved to ready queue", t->name);
	}

	if(t->state == PTH_STATE_WAITING)
    {
		pth_pqueue_insert(&LSCHED->pth_WQ, t->prio, t);
	    pth_debug2("pth_scheduler: thread \"%s\" moved to wait queue", t->name);
	}

	return 0;
}

static int pth_scheduler_handle_received(void)
{
    int ret;
    pth_t t;

    while((ret = pth_sc(read)(LSCHED->pth_receivepipe[0], &t, sizeof(t)))==sizeof(t))
    {
		pth_debug4("%s: scheduler %d received a thread(s)\n",
						__func__, scheduler_id, t);
        __move_threads_queue(t, TRUE);
		LSCHED->nb_threads ++;
	}

    if(ret<sizeof(t) && ret!=0)
        return pth_error(FALSE, errno);

    return 0;
}

static void pth_scheduler_balance(void)
{
	int diff = 0;

	int next_balancer=-1;
	int next_best_balancer=-1;
	int found;
	int ret;
	int rec;
	int i;

	/* create/destroy new scheduler if requested.	*
	 * We do it here because the number is dynamic. */
	if(nb_schedulers != nb_schedulers_requested)
		update_schedulers(nb_schedulers_requested);

	return;
}

static int __distrib_new_threads(struct scheduler_s *dest, int max)
{
	int num=0;
	pth_t t;

	while ((t = pth_pqueue_tail(&LSCHED->pth_NQ)) != NULL)
	{
		if(num>=max)
			goto ret;
        if(t == LSCHED->pth_main)
            continue;
	    pth_pqueue_delete(&LSCHED->pth_NQ, t);
	    //pth_pqueue_insert(&dest->pth_receive, t->prio, t);
        pth_sc(write)(dest->pth_receivepipe[1], &t, sizeof(t));
		num++;
        pth_debug4("%s:%d: Thread %p sent\n", __func__, scheduler_id, t);
	}

ret:
    return num;
}


void check_new_threads()
{
    pth_t t;
    int i;
    int num;
    int ret;
    int found;
    int sshare;

    num = pth_pqueue_elements(&LSCHED->pth_NQ);
    if(num <= 0)
       goto ret;

    sshare = num/nb_schedulers;
    pth_debug6("%s:%d: (new thds %d) each scheduler share is %d (schedulers %d)\n",
          __func__,  scheduler_id, num, sshare, nb_schedulers);

    //distribute some new threads to other schedulers
    found = 0;
    for(i=0; i<MAX_SCHEDULER; i++)
    {
        //local scheduler share is handled below
        if(i==scheduler_id)
            continue;

        if(scheduler_ids[i]>0 || scheduler_ids[i]==-2)
        {
            //pth_debug5("%s:%d: Moving %d thread to %d\n", __func__, scheduler_id, ret, i);
            pth_debug5("%s:%d: Moving %d new thread(s) to %d\n", __func__, scheduler_id, sshare, i);

            ret = __distrib_new_threads(&schedulers[i], sshare);
            LSCHED->nb_threads -= ret;

            found++;
        }
        /* if we visited all existing schedulers */
        if((found-1)==nb_schedulers)
            break;
    }

    // move remaining thread to RQ
    while ((t = pth_pqueue_tail(&LSCHED->pth_NQ)) != NULL) {
        pth_pqueue_delete(&LSCHED->pth_NQ, t);
        t->state = PTH_STATE_READY;
        if (LSCHED->pth_favournew)
            pth_pqueue_insert(&LSCHED->pth_RQ, pth_pqueue_favorite_prio(&LSCHED->pth_RQ), t);
        else
            pth_pqueue_insert(&LSCHED->pth_RQ, PTH_PRIO_STD, t);
        //pth_debug2("pth_scheduler: new thread \"%s\" moved to top of ready queue", t->name);
        //pth_debug2("pth_scheduler: new thread \"%s\" (%p) moved to top of ready queue\n", t->name, t);
        pth_debug4("%s: new thread \"%s\" (%p) moved to top of ready queue\n", __func__, t->name, t);
    }
ret:
    return;
}

/* the heart of this library: the thread scheduler */
intern void *pth_scheduler(void *dummy)
{
    sigset_t sigs;
    pth_time_t running;
    pth_time_t snapshot;
    struct sigaction sa;
    sigset_t ss;
    int sig;

    /*
     * bootstrapping
     */
    pth_debug1("pth_scheduler: bootstrapping");

    /* mark this thread as the special scheduler thread */
    LSCHED->pth_sched->state = PTH_STATE_SCHEDULER;

    /* block all signals in the scheduler thread */
    sigfillset(&sigs);
    pth_sc(sigprocmask)(SIG_SETMASK, &sigs, NULL);

    /* initialize the snapshot time for bootstrapping the loop */
    pth_time_set(&snapshot, PTH_TIME_NOW);

    /*
     * endless scheduler loop
     */
    for (;;) {

        /*
         * Move threads from new queue to ready queue and optionally
         * give them maximum priority so they start immediately.
         */
        check_new_threads();

        /*
         * balance load between schedulers, and expand new ones if necessary.
         */
        pth_debug3("sched_id %d, balance_id %d\n", scheduler_id, balancer_id);
        /* TODO: reduce the number of calling to this libarary */

	    //if(scheduler_id == balancer_id)
            pth_scheduler_handle_received();
	    if(scheduler_id == 0)//balancer_id)
	        pth_scheduler_balance();

        /*
         * Update average scheduler load
         */
        pth_scheduler_load(&snapshot);


        /*
         * Find next thread in ready queue
         */
        pth_current = pth_pqueue_delmax(&LSCHED->pth_RQ);
        pth_assert(pth_current->state == PTH_STATE_READY);
/*
        if(pth_current->state != PTH_STATE_READY && pth_current->state != PTH_STATE_SCHEDULER)
        {
                    while(1)
        }
*/
        if (pth_current == NULL) {
            fprintf(stderr, "**Pth** SCHEDULER (%d) INTERNAL ERROR: "
                            "no more thread(s) available to schedule!?!?\n",
				scheduler_id);
            abort();
        }
        pth_debug4("pth_scheduler: thread \"%s\" selected (prio=%d, qprio=%d)",
                   pth_current->name, pth_current->prio, pth_current->q_prio);

        /*
         * Raise additionally thread-specific signals
         * (they are delivered when we switch the context)
         *
         * Situation is ('#' = signal pending):
         *     process pending (LSCHED->pth_sigpending):         ----####
         *     thread pending (pth_current->sigpending): --##--##
         * Result has to be:
         *     process new pending:                      --######
         */
        if (pth_current->sigpendcnt > 0) {
            sigpending(&LSCHED->pth_sigpending);
            for (sig = 1; sig < PTH_NSIG; sig++)
                if (sigismember(&pth_current->sigpending, sig))
                    if (!sigismember(&LSCHED->pth_sigpending, sig))
                        kill(getpid(), sig);
        }

        /*
         * Set running start time for new thread
         * and perform a context switch to it
         */
        pth_debug4("pth_scheduler %d: switching to thread 0x%lx (\"%s\")",
                   scheduler_id, (unsigned long)pth_current, pth_current->name);

        /* update thread times */
        pth_time_set(&pth_current->lastran, PTH_TIME_NOW);

        /* update scheduler times */
        pth_time_set(&running, &pth_current->lastran);
        pth_time_sub(&running, &snapshot);
        pth_time_add(&LSCHED->pth_sched->running, &running);

        /* ** ENTERING THREAD ** - by switching the machine context */
        pth_current->dispatches++;
        pth_mctx_switch(&LSCHED->pth_sched->mctx, &pth_current->mctx);

        /* update scheduler times */
        pth_time_set(&snapshot, PTH_TIME_NOW);
        pth_debug3("pth_scheduler: cameback from thread 0x%lx (\"%s\")",
                   (unsigned long)pth_current, pth_current->name);

        /*
         * Calculate and update the time the previous thread was running
         */
        pth_time_set(&running, &snapshot);
        pth_time_sub(&running, &pth_current->lastran);
        pth_time_add(&pth_current->running, &running);
        pth_debug3("pth_scheduler: thread \"%s\" ran %.6f",
                   pth_current->name, pth_time_t2d(&running));

        /*
         * Remove still pending thread-specific signals
         * (they are re-delivered next time)
         *
         * Situation is ('#' = signal pending):
         *     thread old pending (pth_current->sigpending): --##--##
         *     process old pending (LSCHED->pth_sigpending):         ----####
         *     process still pending (sigstillpending):      ---#-#-#
         * Result has to be:
         *     process new pending:                          -----#-#
         *     thread new pending (pth_current->sigpending): ---#---#
         */
        if (pth_current->sigpendcnt > 0) {
            sigset_t sigstillpending;
            sigpending(&sigstillpending);
            for (sig = 1; sig < PTH_NSIG; sig++) {
                if (sigismember(&pth_current->sigpending, sig)) {
                    if (!sigismember(&sigstillpending, sig)) {
                        /* thread (and perhaps also process) signal delivered */
                        sigdelset(&pth_current->sigpending, sig);
                        pth_current->sigpendcnt--;
                    }
                    else if (!sigismember(&LSCHED->pth_sigpending, sig)) {
                        /* thread signal not delivered */
                        pth_util_sigdelete(sig);
                    }
                }
            }
        }

        /*
         * Check for stack overflow
         */
        if (pth_current->stackguard != NULL) {
            if (*pth_current->stackguard != 0xDEAD) {
                pth_debug3("pth_scheduler: stack overflow detected for thread 0x%lx (\"%s\")",
                           (unsigned long)pth_current, pth_current->name);
                /*
                 * if the application doesn't catch SIGSEGVs, we terminate
                 * manually with a SIGSEGV now, but output a reasonable message.
                 */
                if (sigaction(SIGSEGV, NULL, &sa) == 0) {
                    if (sa.sa_handler == SIG_DFL) {
                        fprintf(stderr, "**Pth** STACK OVERFLOW: thread pid_t=0x%lx, name=\"%s\"\n",
                                (unsigned long)pth_current, pth_current->name);
                        kill(getpid(), SIGSEGV);
                        sigfillset(&ss);
                        sigdelset(&ss, SIGSEGV);
                        sigsuspend(&ss);
                        abort();
                    }
                }
                /*
                 * else we terminate the thread only and send us a SIGSEGV
                 * which allows the application to handle the situation...
                 */
                pth_current->join_arg = (void *)0xDEAD;
                pth_current->state = PTH_STATE_DEAD;
                kill(getpid(), SIGSEGV);
            }
        }

        /*
         * If previous thread is now marked as dead, kick it out
         */
        if (pth_current->state == PTH_STATE_DEAD || pth_current->state == PTH_STATE_JOINED ) {
            pth_debug2("pth_scheduler: marking thread \"%s\" as dead", pth_current->name);
            //pth_pqueue_delete(&LSCHED->pth_RQ, pth_current);
            if (!pth_current->joinable)
                pth_tcb_free(pth_current);
            //else
            //    pth_pqueue_insert(&LSCHED->pth_DQ, PTH_PRIO_STD, pth_current);
            pth_current = NULL;
        }

        /*
         * If thread wants to wait for an event
         * move it to waiting queue now
         */
        if (pth_current != NULL && pth_current->state == PTH_STATE_WAITING) {
            pth_debug2("pth_scheduler: moving thread \"%s\" to waiting queue",
                       pth_current->name);
            pth_pqueue_insert(&LSCHED->pth_WQ, pth_current->prio, pth_current);
            pth_current = NULL;
        }

        /*
         * migrate old threads in ready queue into higher
         * priorities to avoid starvation and insert last running
         * thread back into this queue, too.
         */
        pth_pqueue_increase(&LSCHED->pth_RQ);
        if (pth_current != NULL)
        {
            assert(pth_current->state == PTH_STATE_READY);
            pth_pqueue_insert(&LSCHED->pth_RQ, pth_current->prio, pth_current);
        }

        /*
         * Manage the events in the waiting queue, i.e. decide whether their
         * events occurred and move them to the ready queue. But wait only if
         * we have already no new or ready threads.
         */
        if (   pth_pqueue_elements(&LSCHED->pth_RQ) == 0
            && pth_pqueue_elements(&LSCHED->pth_NQ) == 0)
            /* still no NEW or READY threads, so we have to wait for new work */
            pth_sched_eventmanager(&snapshot, FALSE /* wait */);
        else
            /* already NEW or READY threads exists, so just poll for even more work */
            pth_sched_eventmanager(&snapshot, TRUE  /* poll */);
    }

    /* NOTREACHED */
    return NULL;
}

/*
 * Look whether some events already occurred (or failed) and move
 * corresponding threads from waiting queue back to ready queue.
 */
intern void pth_sched_eventmanager(pth_time_t *now, int dopoll)
{
    pth_t nexttimer_thread;
    pth_event_t nexttimer_ev;
    pth_time_t nexttimer_value;
    pth_event_t evh;
    pth_event_t ev;
    pth_t t;
    pth_t tlast;
    int this_occurred;
    int any_occurred;
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    struct timeval delay;
    struct timeval *pdelay;
    sigset_t oss;
    struct sigaction sa;
    struct sigaction osa[1+PTH_NSIG];
    char minibuf[128];
    int loop_repeat;
    int fdmax;
    int rc;
    int sig;
    int n;

    pth_debug2("pth_sched_eventmanager: enter in %s mode",
               dopoll ? "polling" : "waiting");

    /* entry point for internal looping in event handling */
    loop_entry:
    loop_repeat = FALSE;

    /* initialize fd sets */
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    fdmax = -1;

    /* initialize signal status */
    sigpending(&LSCHED->pth_sigpending);
    sigfillset(&LSCHED->pth_sigblock);
    sigemptyset(&LSCHED->pth_sigcatch);
    sigemptyset(&LSCHED->pth_sigraised);

    /* initialize next timer */
    pth_time_set(&nexttimer_value, PTH_TIME_ZERO);
    nexttimer_thread = NULL;
    nexttimer_ev = NULL;

    /* for all threads in the waiting queue... */
    any_occurred = FALSE;
    for (t = pth_pqueue_head(&LSCHED->pth_WQ); t != NULL;
         t = pth_pqueue_walk(&LSCHED->pth_WQ, t, PTH_WALK_NEXT)) {

        /* determine signals we block */
        for (sig = 1; sig < PTH_NSIG; sig++)
            if (!sigismember(&(t->mctx.sigs), sig))
                sigdelset(&LSCHED->pth_sigblock, sig);

        /* cancellation support */
        if (t->cancelreq == TRUE)
            any_occurred = TRUE;

        /* ... and all their events... */
        if (t->events == NULL)
            continue;
        /* ...check whether events occurred */
        ev = evh = t->events;
        do {
            if (ev->ev_status == PTH_STATUS_PENDING) {
                this_occurred = FALSE;

                /* Filedescriptor I/O */
                if (ev->ev_type == PTH_EVENT_FD) {
                    /* filedescriptors are checked later all at once.
                       Here we only assemble them in the fd sets */
                    if (ev->ev_goal & PTH_UNTIL_FD_READABLE)
                        FD_SET(ev->ev_args.FD.fd, &rfds);
                    if (ev->ev_goal & PTH_UNTIL_FD_WRITEABLE)
                        FD_SET(ev->ev_args.FD.fd, &wfds);
                    if (ev->ev_goal & PTH_UNTIL_FD_EXCEPTION)
                        FD_SET(ev->ev_args.FD.fd, &efds);
                    if (fdmax < ev->ev_args.FD.fd)
                        fdmax = ev->ev_args.FD.fd;
                }
                /* Filedescriptor Set Select I/O */
                else if (ev->ev_type == PTH_EVENT_SELECT) {
                    /* filedescriptors are checked later all at once.
                       Here we only merge the fd sets. */
                    pth_util_fds_merge(ev->ev_args.SELECT.nfd,
                                       ev->ev_args.SELECT.rfds, &rfds,
                                       ev->ev_args.SELECT.wfds, &wfds,
                                       ev->ev_args.SELECT.efds, &efds);
                    if (fdmax < ev->ev_args.SELECT.nfd-1)
                        fdmax = ev->ev_args.SELECT.nfd-1;
                }
                /* Signal Set */
                else if (ev->ev_type == PTH_EVENT_SIGS) {
                    for (sig = 1; sig < PTH_NSIG; sig++) {
                        if (sigismember(ev->ev_args.SIGS.sigs, sig)) {
                            /* thread signal handling */
                            if (sigismember(&t->sigpending, sig)) {
                                *(ev->ev_args.SIGS.sig) = sig;
                                sigdelset(&t->sigpending, sig);
                                t->sigpendcnt--;
                                this_occurred = TRUE;
                            }
                            /* process signal handling */
                            if (sigismember(&LSCHED->pth_sigpending, sig)) {
                                if (ev->ev_args.SIGS.sig != NULL)
                                    *(ev->ev_args.SIGS.sig) = sig;
                                pth_util_sigdelete(sig);
                                sigdelset(&LSCHED->pth_sigpending, sig);
                                this_occurred = TRUE;
                            }
                            else {
                                sigdelset(&LSCHED->pth_sigblock, sig);
                                sigaddset(&LSCHED->pth_sigcatch, sig);
                            }
                        }
                    }
                }
                /* Timer */
                else if (ev->ev_type == PTH_EVENT_TIME) {
                    if (pth_time_cmp(&(ev->ev_args.TIME.tv), now) < 0)
                        this_occurred = TRUE;
                    else {
                        /* remember the timer which will be elapsed next */
                        if ((nexttimer_thread == NULL && nexttimer_ev == NULL) ||
                            pth_time_cmp(&(ev->ev_args.TIME.tv), &nexttimer_value) < 0) {
                            nexttimer_thread = t;
                            nexttimer_ev = ev;
                            pth_time_set(&nexttimer_value, &(ev->ev_args.TIME.tv));
                        }
                    }
                }
                /* Message Port Arrivals */
                else if (ev->ev_type == PTH_EVENT_MSG) {
                    if (pth_ring_elements(&(ev->ev_args.MSG.mp->mp_queue)) > 0)
                        this_occurred = TRUE;
                }
                /* Mutex Release */
                else if (ev->ev_type == PTH_EVENT_MUTEX) {
                    if (!(ev->ev_args.MUTEX.mutex->mx_state & PTH_MUTEX_LOCKED))
                        this_occurred = TRUE;
                }
                /* Condition Variable Signal */
                else if (ev->ev_type == PTH_EVENT_COND) {
                    if (ev->ev_args.COND.cond->cn_state & PTH_COND_SIGNALED) {
                        if (ev->ev_args.COND.cond->cn_state & PTH_COND_BROADCAST)
                            this_occurred = TRUE;
                        else {
                            if (!(ev->ev_args.COND.cond->cn_state & PTH_COND_HANDLED)) {
                                ev->ev_args.COND.cond->cn_state |= PTH_COND_HANDLED;
                                this_occurred = TRUE;
                            }
                        }
                    }
                }
                /* Thread Termination */
                else if (ev->ev_type == PTH_EVENT_TID) {
                    if (   (   ev->ev_args.TID.tid == NULL
                            && pth_pqueue_elements(&LSCHED->pth_DQ) > 0)
                        || (   ev->ev_args.TID.tid != NULL
                            && ev->ev_args.TID.tid->state == ev->ev_goal))
                        this_occurred = TRUE;
                }
                /* Custom Event Function */
                else if (ev->ev_type == PTH_EVENT_FUNC) {
                    if (ev->ev_args.FUNC.func(ev->ev_args.FUNC.arg))
                        this_occurred = TRUE;
                    else {
                        pth_time_t tv;
                        pth_time_set(&tv, now);
                        pth_time_add(&tv, &(ev->ev_args.FUNC.tv));
                        if ((nexttimer_thread == NULL && nexttimer_ev == NULL) ||
                            pth_time_cmp(&tv, &nexttimer_value) < 0) {
                            nexttimer_thread = t;
                            nexttimer_ev = ev;
                            pth_time_set(&nexttimer_value, &tv);
                        }
                    }
                }

                /* tag event if it has occurred */
                if (this_occurred) {
                    pth_debug2("pth_sched_eventmanager: [non-I/O] event occurred for thread \"%s\"", t->name);
                    ev->ev_status = PTH_STATUS_OCCURRED;
                    any_occurred = TRUE;
                }
            }
        } while ((ev = ev->ev_next) != evh);
    }
    if (any_occurred)
        dopoll = TRUE;

    /* now decide how to poll for fd I/O and timers */
    if (dopoll) {
        /* do a polling with immediate timeout,
           i.e. check the fd sets only without blocking */
        pth_time_set(&delay, PTH_TIME_ZERO);
        pdelay = &delay;
    }
    else if (nexttimer_ev != NULL) {
        /* do a polling with a timeout set to the next timer,
           i.e. wait for the fd sets or the next timer */
        pth_time_set(&delay, &nexttimer_value);
        pth_time_sub(&delay, now);
        pdelay = &delay;
    }
    else {
        /* do a polling without a timeout,
           i.e. wait for the fd sets only with blocking */
        pdelay = NULL;
    }

#if 1
    /* use join pipe */
    if(!dopoll)
    {
        FD_SET(pth_joinpipe[0], &rfds);
        if (fdmax < pth_joinpipe[0])
            fdmax = pth_joinpipe[0];
    }
#endif

    if(!dopoll)
    {
        FD_SET(LSCHED->pth_receivepipe[0], &rfds);
        if (fdmax < LSCHED->pth_receivepipe[0])
            fdmax = LSCHED->pth_receivepipe[0];
    }

    /* clear pipe and let select() wait for the read-part of the pipe */
    while (pth_sc(read)(LSCHED->pth_sigpipe[0], minibuf, sizeof(minibuf)) > 0) ;
    FD_SET(LSCHED->pth_sigpipe[0], &rfds);
    if (fdmax < LSCHED->pth_sigpipe[0])
        fdmax = LSCHED->pth_sigpipe[0];

#if 1
    /* replace signal actions for signals we've to catch for events */
    for (sig = 1; sig < PTH_NSIG; sig++) {
        if (sigismember(&LSCHED->pth_sigcatch, sig)) {
            sa.sa_handler = pth_sched_eventmanager_sighandler;
            sigfillset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(sig, &sa, &osa[sig]);
        }
    }

    /* allow some signals to be delivered: Either to our
       catching handler or directly to the configured
       handler for signals not catched by events */
    pth_sc(sigprocmask)(SIG_SETMASK, &LSCHED->pth_sigblock, &oss);
#endif

    /* now do the polling for filedescriptor I/O and timers
       WHEN THE SCHEDULER SLEEPS AT ALL, THEN HERE!! */
    rc = -1;
    if (!(dopoll && fdmax == -1))
        while ((rc = pth_sc(select)(fdmax+1, &rfds, &wfds, &efds, pdelay)) < 0
               && errno == EINTR) ;
#if 1
    /* restore signal mask and actions and handle signals */
    pth_sc(sigprocmask)(SIG_SETMASK, &oss, NULL);
    for (sig = 1; sig < PTH_NSIG; sig++)
        if (sigismember(&LSCHED->pth_sigcatch, sig))
            sigaction(sig, &osa[sig], NULL);
#endif

    /* if the timer elapsed, handle it */
    if (!dopoll && rc == 0 && nexttimer_ev != NULL) {
        if (nexttimer_ev->ev_type == PTH_EVENT_FUNC) {
            /* it was an implicit timer event for a function event,
               so repeat the event handling for rechecking the function */
            loop_repeat = TRUE;
        }
        else {
            /* it was an explicit timer event, standing for its own */
            pth_debug2("pth_sched_eventmanager: [timeout] event occurred for thread \"%s\"",
                       nexttimer_thread->name);
            nexttimer_ev->ev_status = PTH_STATUS_OCCURRED;
        }
    }

#if 1
    /* if the internal signal pipe was used, adjust the select() results */
    if (!dopoll && rc > 0 && FD_ISSET(LSCHED->pth_sigpipe[0], &rfds)) {
        pth_debug1("pth_sched_eventmanager: sig pipe is set");
        FD_CLR(LSCHED->pth_sigpipe[0], &rfds);
        rc--;
    }
#endif

#if 1
    /* if the internal signal pipe was used, adjust the select() results */
    if (!dopoll && rc > 0 && FD_ISSET(pth_joinpipe[0], &rfds)) {
        void *tid;
        int pp_ret;
        loop_repeat = TRUE;
        pp_ret = pth_sc(read)(pth_joinpipe[0], &tid, sizeof(tid));
        pth_debug2("pth_sched_eventmanager: join pipe is set, read %d", pp_ret);
        if(pp_ret<0)
            perror("pipe after select");
        FD_CLR(pth_joinpipe[0], &rfds);
        rc--;
    }
#endif

    if (!dopoll && rc > 0 && FD_ISSET(LSCHED->pth_receivepipe[0], &rfds)) {
        //void *tid;
        //pth_sc(read)(pth_receivepipe[1], &tid, sizeof(tid));
        pth_debug1("pth_sched_eventmanager: receive pipe is set");
        FD_CLR(LSCHED->pth_receivepipe[0], &rfds);
        rc--;
    }

    /* if an error occurred, avoid confusion in the cleanup loop */
    if (rc <= 0) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
    }

    /* now comes the final cleanup loop where we've to
       do two jobs: first we've to do the late handling of the fd I/O events and
       additionally if a thread has one occurred event, we move it from the
       waiting queue to the ready queue */

    /* for all threads in the waiting queue... */
    t = pth_pqueue_head(&LSCHED->pth_WQ);
    while (t != NULL) {

        /* do the late handling of the fd I/O and signal
           events in the waiting event ring */
        any_occurred = FALSE;
        if (t->events != NULL) {
            ev = evh = t->events;
            do {
                /*
                 * Late handling for still not occured events
                 */
                if (ev->ev_status == PTH_STATUS_PENDING) {
                    /* Filedescriptor I/O */
                    if (ev->ev_type == PTH_EVENT_FD) {
                        if (   (   ev->ev_goal & PTH_UNTIL_FD_READABLE
                                && FD_ISSET(ev->ev_args.FD.fd, &rfds))
                            || (   ev->ev_goal & PTH_UNTIL_FD_WRITEABLE
                                && FD_ISSET(ev->ev_args.FD.fd, &wfds))
                            || (   ev->ev_goal & PTH_UNTIL_FD_EXCEPTION
                                && FD_ISSET(ev->ev_args.FD.fd, &efds)) ) {
                            pth_debug2("pth_sched_eventmanager: "
                                       "[I/O] event occurred for thread \"%s\"", t->name);
                            ev->ev_status = PTH_STATUS_OCCURRED;
                        }
                        else if (rc < 0) {
                            /* re-check particular filedescriptor */
                            int rc2;
                            if (ev->ev_goal & PTH_UNTIL_FD_READABLE)
                                FD_SET(ev->ev_args.FD.fd, &rfds);
                            if (ev->ev_goal & PTH_UNTIL_FD_WRITEABLE)
                                FD_SET(ev->ev_args.FD.fd, &wfds);
                            if (ev->ev_goal & PTH_UNTIL_FD_EXCEPTION)
                                FD_SET(ev->ev_args.FD.fd, &efds);
                            pth_time_set(&delay, PTH_TIME_ZERO);
                            while ((rc2 = pth_sc(select)(ev->ev_args.FD.fd+1, &rfds, &wfds, &efds, &delay)) < 0
                                   && errno == EINTR) ;
                            if (rc2 > 0) {
                                /* cleanup afterwards for next iteration */
                                FD_CLR(ev->ev_args.FD.fd, &rfds);
                                FD_CLR(ev->ev_args.FD.fd, &wfds);
                                FD_CLR(ev->ev_args.FD.fd, &efds);
                            } else if (rc2 < 0) {
                                /* cleanup afterwards for next iteration */
                                FD_ZERO(&rfds);
                                FD_ZERO(&wfds);
                                FD_ZERO(&efds);
                                ev->ev_status = PTH_STATUS_FAILED;
                                pth_debug2("pth_sched_eventmanager: "
                                           "[I/O] event failed for thread \"%s\"", t->name);
                            }
                        }
                    }
                    /* Filedescriptor Set I/O */
                    else if (ev->ev_type == PTH_EVENT_SELECT) {
                        if (pth_util_fds_test(ev->ev_args.SELECT.nfd,
                                              ev->ev_args.SELECT.rfds, &rfds,
                                              ev->ev_args.SELECT.wfds, &wfds,
                                              ev->ev_args.SELECT.efds, &efds)) {
                            n = pth_util_fds_select(ev->ev_args.SELECT.nfd,
                                                    ev->ev_args.SELECT.rfds, &rfds,
                                                    ev->ev_args.SELECT.wfds, &wfds,
                                                    ev->ev_args.SELECT.efds, &efds);
                            if (ev->ev_args.SELECT.n != NULL)
                                *(ev->ev_args.SELECT.n) = n;
                            ev->ev_status = PTH_STATUS_OCCURRED;
                            pth_debug2("pth_sched_eventmanager: "
                                       "[I/O] event occurred for thread \"%s\"", t->name);
                        }
                        else if (rc < 0) {
                            /* re-check particular filedescriptor set */
                            int rc2;
                            fd_set *prfds = NULL;
                            fd_set *pwfds = NULL;
                            fd_set *pefds = NULL;
                            fd_set trfds;
                            fd_set twfds;
                            fd_set tefds;
                            if (ev->ev_args.SELECT.rfds) {
                                memcpy(&trfds, ev->ev_args.SELECT.rfds, sizeof(rfds));
                                prfds = &trfds;
                            }
                            if (ev->ev_args.SELECT.wfds) {
                                memcpy(&twfds, ev->ev_args.SELECT.wfds, sizeof(wfds));
                                pwfds = &twfds;
                            }
                            if (ev->ev_args.SELECT.efds) {
                                memcpy(&tefds, ev->ev_args.SELECT.efds, sizeof(efds));
                                pefds = &tefds;
                            }
                            pth_time_set(&delay, PTH_TIME_ZERO);
                            while ((rc2 = pth_sc(select)(ev->ev_args.SELECT.nfd+1, prfds, pwfds, pefds, &delay)) < 0
                                   && errno == EINTR) ;
                            if (rc2 < 0) {
                                ev->ev_status = PTH_STATUS_FAILED;
                                pth_debug2("pth_sched_eventmanager: "
                                           "[I/O] event failed for thread \"%s\"", t->name);
                            }
                        }
                    }
                    /* Signal Set */
                    else if (ev->ev_type == PTH_EVENT_SIGS) {
                        for (sig = 1; sig < PTH_NSIG; sig++) {
                            if (sigismember(ev->ev_args.SIGS.sigs, sig)) {
                                if (sigismember(&LSCHED->pth_sigraised, sig)) {
                                    if (ev->ev_args.SIGS.sig != NULL)
                                        *(ev->ev_args.SIGS.sig) = sig;
                                    pth_debug2("pth_sched_eventmanager: "
                                               "[signal] event occurred for thread \"%s\"", t->name);
                                    sigdelset(&LSCHED->pth_sigraised, sig);
                                    ev->ev_status = PTH_STATUS_OCCURRED;
                                }
                            }
                        }
                    }
                }
                /*
                 * post-processing for already occured events
                 */
                else {
                    /* Condition Variable Signal */
                    if (ev->ev_type == PTH_EVENT_COND) {
                        /* clean signal */
                        if (ev->ev_args.COND.cond->cn_state & PTH_COND_SIGNALED) {
                            ev->ev_args.COND.cond->cn_state &= ~(PTH_COND_SIGNALED);
                            ev->ev_args.COND.cond->cn_state &= ~(PTH_COND_BROADCAST);
                            ev->ev_args.COND.cond->cn_state &= ~(PTH_COND_HANDLED);
                        }
                    }
                }

                /* local to global mapping */
                if (ev->ev_status != PTH_STATUS_PENDING)
                    any_occurred = TRUE;
            } while ((ev = ev->ev_next) != evh);
        }

        /* cancellation support */
        if (t->cancelreq == TRUE) {
            pth_debug2("pth_sched_eventmanager: cancellation request pending for thread \"%s\"", t->name);
            any_occurred = TRUE;
        }

        /* walk to next thread in waiting queue */
        tlast = t;
        t = pth_pqueue_walk(&LSCHED->pth_WQ, t, PTH_WALK_NEXT);

        /*
         * move last thread to ready queue if any events occurred for it.
         * we insert it with a slightly increased queue priority to it a
         * better chance to immediately get scheduled, else the last running
         * thread might immediately get again the CPU which is usually not
         * what we want, because we oven use pth_yield() calls to give others
         * a chance.
         */
        if (any_occurred) {
            pth_pqueue_delete(&LSCHED->pth_WQ, tlast);
            tlast->state = PTH_STATE_READY;
            pth_pqueue_insert(&LSCHED->pth_RQ, tlast->prio+1, tlast);
            pth_debug2("pth_sched_eventmanager: thread \"%s\" moved from waiting "
                       "to ready queue", tlast->name);
        }
    }

    /* perhaps we have to internally loop... */
    if (loop_repeat) {
        pth_time_set(now, PTH_TIME_NOW);
        goto loop_entry;
    }

    pth_debug1("pth_sched_eventmanager: leaving");
    return;
}

intern void pth_sched_eventmanager_sighandler(int sig)
{
    char c;

    /* remember raised signal */
    sigaddset(&LSCHED->pth_sigraised, sig);

    /* write signal to signal pipe in order to awake the select() */
    c = (int)sig;
    pth_sc(write)(LSCHED->pth_sigpipe[1], &c, sizeof(char));
    return;
}

