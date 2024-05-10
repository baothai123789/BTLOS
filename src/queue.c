#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if (proc == NULL || q == NULL || q->size == MAX_QUEUE_SIZE) return;
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */

        /*
                Input: a ready queue 
                Q's priority is already filtered in Sched.c by proc->prio
                Output:  a pcb whose prioprity is the highest priority
        */
        if (empty(q))
        {
                printf("\nQueue is empty\n");
                return NULL;
        }
        /*
                max_prio_index and max_prio is assumed to be the highest prio queue to be exported
        */
        int max_prio = 0;
        for (int i = max_prio; i < q->size; i++) if(q->proc[i]->priority < q->proc[max_prio]->priority) max_prio = i;
        struct pcb_t *proc = q->proc[max_prio];
        for (int i = max_prio; i < q->size - 1; i++)
                q->proc[i] = q->proc[i + 1];
        q->size--;
        return proc;
}

