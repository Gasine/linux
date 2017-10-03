#include <linux/sched/runq.h>

void runq_add(struct runq * q, struct task_struct *p, int flags)
{
	int pri;
	struct list_head *head;

	pri = p->prio / KTZ_HEADS_PER_RUNQ;
}
