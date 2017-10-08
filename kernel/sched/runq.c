#include <linux/sched/runq.h>

void runq_set_bit(struct runq *q, int pri)
{
	unsigned long bm = q->status;
	bitmap_set(bm, pri, 1);
}

void runq_clear_bit(struct runq *q, int pri)
{
	unsigned long bm = q->status;
	bitmap_clear(bm, pri, 1);
}

void runq_add(struct runq * q, struct task_struct *p, int flags)
{
	int pri;
	pri = p->prio / KTZ_PRIO_PER_QUEUE;
	runq_add_pri(q, p, pri, flags);
}

void runq_add_pri(struct runq * q, struct task_struct *p, int pri, int flags)
{
	struct list_head *head;
	struct sched_ktz_entity *ke = &p->ktz_se;

	ke->rqindex = pri;
	runq_set_bit(q, pri);
	head = &q->queues[pri];
	if (flags & KTZ_PREEMPTED) {
		list_add(&ke->runq, head);	
	}
	else {
		list_add_tail(&ke->runq, head);	
	}
}

/*
 * Remove the thread from the queue specified by its priority, and clear the
 * corresponding status bit if the queue becomes empty.
 * Caller must set state afterwards.
 */
void runq_remove(struct runq *q, struct task_struct *p)
{
	runq_remove_idx(q, p, NULL);
}

void runq_remove_idx(struct runq *q, struct task_struct *p, int *idx)
{
	int pri;
	struct list_head *head;
	struct sched_ktz_entity *ke = &p->ktz_se;

	pri = ke->rqindex;
	head = &q->queues[pri];
	list_del(&ke->runq);
	if (list_empty(head)) {
		runq_clear_bit(q, pri);
		if (idx != NULL && *idx == pri)
			*idx = (pri + 1) % KTZ_HEADS_PER_RUNQ;
	}
}
