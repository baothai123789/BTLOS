
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int mlq_first_init[MAX_PRIO] = {0};
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
	int i ;

	for (i = 0; i < MAX_PRIO; i ++)
		mlq_ready_queue[i].size = 0;
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	unsigned long prio;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 */
	proc = dequeue(&mlq_ready_queue[0]);

	pthread_mutex_lock(&queue_lock);
	//route prio from 0 to MAXPRIO, check unempty Q with available slots then dequeue
	for (prio = 0; prio < MAX_PRIO; prio++) {
    if (!empty(&mlq_ready_queue[prio]) && mlq_ready_queue[prio].slot > 0) {
      // Dequeue the process from the highest priority non-empty queue
      proc = dequeue(&mlq_ready_queue[prio]);
      mlq_ready_queue[prio].slot--;
      break;
    }
	}

	//check if reset slots for all queues is needed (all queues have 0 slot)
	int resetSlots = 1;
	for (prio = 0; prio < MAX_PRIO; prio++) {
    if (mlq_ready_queue[prio].slot != 0) {
      resetSlots = 0;
      break;
    }
	}
	
	if (resetSlots) {
    for (prio = 0; prio < MAX_PRIO; prio++)
      mlq_ready_queue[prio].slot = MAX_PRIO - prio;
	}

	pthread_mutex_lock(&queue_lock);

	return proc;	
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	 if (mlq_ready_queue[proc->prio].size == 1 && mlq_first_init[proc->prio] == 0) {
    mlq_ready_queue[proc->prio].slot = MAX_PRIO - proc->prio;
    mlq_first_init[proc->prio] = 1; // Marked as init -> cannot re-assign slots but reset only
	}
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	proc = dequeue(&ready_queue);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


