
/* TODO: improve by taking half of each queue starting with the ready 	*
 * even better balance using the load of threads 			*/
static int __balance_work(struct scheduler_s *dest, int max)
{
	int num=0;
	pth_t t;

	while ((t = pth_pqueue_tail(&LSCHED->pth_NQ)) != NULL)
	{
		if(num>=max)
			goto exit;
        if(t == LSCHED->pth_main)
            continue;
	    pth_pqueue_delete(&LSCHED->pth_NQ, t);
	    //pth_pqueue_insert(&dest->pth_receive, t->prio, t);
        pth_sc(write)(dest->pth_receivepipe[1], &t, sizeof(t));
		num++;
	}

	while ((t = pth_pqueue_tail(&LSCHED->pth_RQ)) != NULL)
	{
		if(num>=max)
			goto exit;
        if(t == LSCHED->pth_main)
            continue;
	    pth_pqueue_delete(&LSCHED->pth_RQ, t);
	    //pth_pqueue_insert(&dest->pth_receive, t->prio, t);
        pth_sc(write)(dest->pth_receivepipe[1], &t, sizeof(t));
		num++;
	}

	while ((t = pth_pqueue_tail(&LSCHED->pth_WQ)) != NULL)
	{
		if(num>=max)
			goto exit;
        if(t == LSCHED->pth_main)
            continue;
	    pth_pqueue_delete(&LSCHED->pth_WQ, t);
	    //pth_pqueue_insert(&dest->pth_receive, t->prio, t);
        pth_sc(write)(dest->pth_receivepipe[1], &t, sizeof(t));
		num++;
	}

exit:
	return num;
}

static int move_threads_queue(pth_pqueue_t *from, int boost)
{
	int ret=0;
    	pth_t t;
	/*
	 * Move threads from the "from" queue to the LSCHED queues depending on the state
	 */
	while ((t = pth_pqueue_tail(from)) != NULL)
    {
	    pth_pqueue_delete(from, t);
        __move_threads_queue(t, boost);
	    pth_debug2("pth_scheduler: thread \"%s\" moved to queue", t->name);
	    ret++;
	}
	return ret;
}
#if 0
	/* balance the work by giving some */
	found=0;
	for(i=0; i<MAX_SCHEDULER; i++)
	{
		if(i==scheduler_id)
			continue;

		if(scheduler_ids[i]>0 || scheduler_ids[i]==-2)
		{
			if(!schedulers[i].new)
				next_balancer=i;

			diff = LSCHED->nb_threads - schedulers[i].nb_threads;
			ret = diff/2;

			//pth_debug6
            printf("%s:%d: remote contains %d thread. Can move %d. Diff is %d\n",
				__func__, scheduler_id, schedulers[i].nb_threads, ret, diff);

			//ret = schedulers[i].pth_loadval - LSCHED->pth_loadval; TODO
			if(ret>0)
			{
				//pth_debug5("%s:%d: Moving %d thread to %d\n", __func__, scheduler_id, ret, i);
				printf("%s:%d: Moving %d thread to %d\n", __func__, scheduler_id, ret, i);

				ret = __balance_work(&schedulers[i], ret);
				LSCHED->nb_threads -= ret;

				next_best_balancer = i;

				break; /* gives remote a chance to update its nb_thread value */
			}
			found++;
		}
		/* if we visited all existing schedulers */
		if((found-1)==nb_schedulers)
			break;
	}

	//Write barrier if arch writes are not ordered (maybe in ARM/not in x86)

	/* set the next balancer_id */
	if(next_best_balancer>0)
		balancer_id= next_best_balancer; /* so the remote can update his nb_threads */
	else if(next_balancer>0)
		balancer_id= next_balancer;

#else

    found=0;
    //if((LSCHED->nb_threads-1) != diff)//>= total_nb_threads)
    //if(sched_share==0)//>= total_nb_threads)
    if(sched_share != (LSCHED->nb_threads-1)) //(total_nb_threads-nb_schedulers)/nb_schedulers)
    {
        //sched_share = LSCHED->nb_threads/nb_schedulers;
        sched_share = (total_nb_threads-nb_schedulers)/nb_schedulers;
        //pth_debug4
        printf("%s:%d: (lnbt %d) each scheduler share is %d (total %d/schedulers %d)\n",
                    __func__,  scheduler_id, LSCHED->nb_threads, sched_share, total_nb_threads, nb_schedulers);
        if(sched_share<=0)
            goto exit;

        for(i=0; i<MAX_SCHEDULER; i++)
        {
            if(i==scheduler_id)
                continue;

            if(scheduler_ids[i]>0 || scheduler_ids[i]==-2)
            {
                //pth_debug5("%s:%d: Moving %d thread to %d\n", __func__, scheduler_id, ret, i);
                printf("%s:%d: Moving %d thread to %d\n", __func__, scheduler_id, sched_share, i);

                ret = __balance_work(&schedulers[i], sched_share);
                LSCHED->nb_threads -= ret;

                found++;
            }
            /* if we visited all existing schedulers */
            if((found-1)==nb_schedulers)
                break;
        }

    }
#endif
