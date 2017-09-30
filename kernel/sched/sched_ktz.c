/*
 * ktz scheduling class, mapped to range a of 5 levels of SCHED_NORMAL policy
 */

#include "sched.h"

/*
 * Timeslice and age threshold are repsented in jiffies. Default timeslice
 * is 100ms. Both parameters can be tuned from /proc/sys/kernel.
 */

#define KTZ_TIMESLICE		(100 * HZ / 1000)
#define KTZ_AGE_THRESHOLD	(3 * KTZ_TIMESLICE)

unsigned int sysctl_sched_ktz_timeslice = KTZ_TIMESLICE;
static inline unsigned int get_timeslice(void)
{
	return sysctl_sched_ktz_timeslice;
}

unsigned int sysctl_sched_ktz_age_threshold = KTZ_AGE_THRESHOLD;
static inline unsigned int get_age_threshold(void)
{
	return sysctl_sched_ktz_age_threshold;
}

/*
 * Init
 */

void init_ktz_tdq(struct ktz_tdq *ktz_tdq)
{
	INIT_LIST_HEAD(&ktz_tdq->queue);
}

/*
 * Helper functions
 */

static inline struct task_struct *ktz_task_of(struct sched_ktz_entity *ktz_se)
{
	return container_of(ktz_se, struct task_struct, ktz_se);
}

static inline void _enqueue_task_ktz(struct rq *rq, struct task_struct *p)
{
	struct sched_ktz_entity *ktz_se = &p->ktz_se;
	struct list_head *queue = &rq->ktz.queue;
	list_add_tail(&ktz_se->run_list, queue);
}

static inline void _dequeue_task_ktz(struct task_struct *p)
{
	struct sched_ktz_entity *ktz_se = &p->ktz_se;
	list_del_init(&ktz_se->run_list);
}

/*
 * Scheduling class functions to implement
 */

static void enqueue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	_enqueue_task_ktz(rq, p);
	add_nr_running(rq,1);
}

static void dequeue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	_dequeue_task_ktz(p);
	sub_nr_running(rq,1);
}

static void yield_task_ktz(struct rq *rq)
{
}

static void check_preempt_curr_ktz(struct rq *rq, struct task_struct *p, int flags)
{
}

static struct task_struct *pick_next_task_ktz(struct rq *rq, struct task_struct* prev, struct rq_flags *flags)
{
	struct ktz_tdq *ktz_tdq = &rq->ktz;
	struct sched_ktz_entity *next;
	if(!list_empty(&ktz_tdq->queue)) {
		next = list_first_entry(&ktz_tdq->queue, struct sched_ktz_entity, run_list);
                put_prev_task(rq, prev);
		return ktz_task_of(next);
	} else {
		return NULL;
	}
}

static void put_prev_task_ktz(struct rq *rq, struct task_struct *prev)
{
}

static void set_curr_task_ktz(struct rq *rq)
{
}

static void task_tick_ktz(struct rq *rq, struct task_struct *curr, int queued)
{
}

static void switched_from_ktz(struct rq *rq, struct task_struct *p)
{
}

static void switched_to_ktz(struct rq *rq, struct task_struct *p)
{
}

static void prio_changed_ktz(struct rq*rq, struct task_struct *p, int oldprio)
{
}

static unsigned int get_rr_interval_ktz(struct rq* rq, struct task_struct *p)
{
	return get_timeslice();
}
#ifdef CONFIG_SMP
/*
 * SMP related functions	
 */

static inline int select_task_rq_ktz(struct task_struct *p, int cpu, int sd_flags, int wake_flags)
{
	int new_cpu = smp_processor_id();
	
	return new_cpu; //set assigned CPU to zero
}


static void set_cpus_allowed_ktz(struct task_struct *p,  const struct cpumask *new_mask)
{
}
#endif
/*
 * Scheduling class
 */
static void update_curr_ktz(struct rq*rq)
{
}
const struct sched_class ktz_sched_class = {
	.next			= &idle_sched_class,
	.enqueue_task		= enqueue_task_ktz,
	.dequeue_task		= dequeue_task_ktz,
	.yield_task		= yield_task_ktz,

	.check_preempt_curr	= check_preempt_curr_ktz,
	
	.pick_next_task		= pick_next_task_ktz,
	.put_prev_task		= put_prev_task_ktz,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_ktz,
	.set_cpus_allowed	= set_cpus_allowed_ktz,
#endif

	.set_curr_task		= set_curr_task_ktz,
	.task_tick		= task_tick_ktz,

	.switched_from		= switched_from_ktz,
	.switched_to		= switched_to_ktz,
	.prio_changed		= prio_changed_ktz,

	.get_rr_interval	= get_rr_interval_ktz,
	.update_curr		= update_curr_ktz,
};
