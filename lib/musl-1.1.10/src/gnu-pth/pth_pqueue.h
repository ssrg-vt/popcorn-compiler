#ifndef _PTH_PQ_H_
#define _PTH_PQ_H_

/* thread priority queue */
struct pth_pqueue_st {
    pth_t q_head;
    int   q_num;
};
typedef struct pth_pqueue_st pth_pqueue_t;

#endif
