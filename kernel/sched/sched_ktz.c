/*
 * ktz scheduling class, mapped to range a of 5 levels of SCHED_NORMAL policy
 */

#include "sched.h"

/*
 * Timeslice and age threshold are repsented in jiffies. Default timeslice
 * is 100ms. Both parameters can be tuned from /proc/sys/kernel.
 */

#define LOG(...) \
	printk_deferred(__VA_ARGS__)

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

static void update_runtime(struct task_struct *p)
{
	unsigned long long delta_exec;
	unsigned long long now;
	struct sched_ktz_entity *ke = &p->ktz_se;

	now = sched_clock();
	delta_exec = now - ke->ts_startrun;
	ke->ts_startrun = now;
	ke->ts_runtime += delta_exec;
}

static void start_runtime(struct task_struct *p)
{
	p->ktz_se.ts_startrun = sched_clock();
}

/*
 * Init
 */

void init_ktz_tdq(struct ktz_tdq *ktz_tdq)
{
	printk_deferred("KTZ Scheduler initialized\n");
	printk_deferred("KTZ_HEADS_PER_RUNQ	= %d\n", KTZ_HEADS_PER_RUNQ);
	printk_deferred("KTZ_RUNQ_BITMAP_SIZE	= %ld\n", KTZ_RUNQ_BITMAP_SIZE);
	printk_deferred("task_struct size	= %ld\n", sizeof(struct task_struct));
	INIT_LIST_HEAD(&ktz_tdq->queue);
}

static void print_times(const char *prefix, struct task_struct *p)
{
	struct sched_ktz_entity ke = p->ktz_se;
	unsigned long long slp = ke.ts_slptime / 1000000;
	unsigned long long run = ke.ts_runtime / 1000000;
	printk_deferred("%s : Task %d, slptime = %llu ms, runtime = %llu ms\n", prefix, p->pid, slp, run);
}

/*
 * Helper functions
 */

static inline struct task_struct *ktz_task_of(struct sched_ktz_entity *ktz_se)
{
	return container_of(ktz_se, struct task_struct, ktz_se);
}

/*
 * Scheduling class functions to implement
 */

static void enqueue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	LOG("Entering : enqueue_task_ktz\n");
	struct sched_ktz_entity *ktz_se = &p->ktz_se;
	struct list_head *queue = &rq->ktz.queue;
	struct sched_ktz_entity *ke = &rq->curr->ktz_se;

	if (flags & ENQUEUE_WAKEUP) {
		LOG("Task %d is waking up\n", p->pid);
		ktz_se->ts_slptime += sched_clock() - ktz_se->ts_startslp;
	}

	add_nr_running(rq,1);
	list_add_tail(&ktz_se->run_list, queue);
	print_times("Enqueue", p);
}

static void dequeue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	LOG("Entering : dequeue_task_ktz\n");
	struct sched_ktz_entity *ktz_se = &p->ktz_se;
	struct sched_ktz_entity *ke = &rq->curr->ktz_se;

	if (flags & DEQUEUE_SLEEP) {
		LOG("Task %d is going to sleep\n", p->pid);
		ktz_se->ts_startslp = sched_clock();
	}
	else {
		update_runtime(p);
	}
	sub_nr_running(rq,1);
	list_del_init(&ktz_se->run_list);
	print_times("Dequeue", p);
}

static void yield_task_ktz(struct rq *rq)
{
	LOG("Entering : yield_task_ktz\n");
}

static void check_preempt_curr_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	LOG("Entering : check_preempt_curr_ktz\n");
}

static struct task_struct *pick_next_task_ktz(struct rq *rq, struct task_struct* prev, struct rq_flags *flags)
{
	LOG("Entering : task_struct *pick_next_task_ktz\n");
	struct ktz_tdq *ktz_tdq = &rq->ktz;
	struct sched_ktz_entity *next;
	struct task_struct *next_task;
	put_prev_task(rq, prev);
	if(!list_empty(&ktz_tdq->queue)) {
		next = list_first_entry(&ktz_tdq->queue, struct sched_ktz_entity, run_list);
		next_task = ktz_task_of(next);
		start_runtime(next);
		LOG("Pick task %d (ran %d ms) ###### ", next_task->pid, next->ts_runtime / 1000000);
		return next_task;
	}
	else {
		LOG("Pick task == NULL");
		return NULL;
	}
}

static void put_prev_task_ktz(struct rq *rq, struct task_struct *prev)
{
	LOG("Entering : put_prev_task_ktz\n");
	if (prev->sched_class == &ktz_sched_class)
		update_runtime(prev);
}

static void set_curr_task_ktz(struct rq *rq)
{
	LOG("Entering : set_curr_task_ktz\n");
}

static void task_tick_ktz(struct rq *rq, struct task_struct *curr, int queued)
{
	LOG("Entering : task_tick_ktz\n");
	LOG("Tick task %d", curr->pid);
}

static void switched_from_ktz(struct rq *rq, struct task_struct *p)
{
	LOG("Entering : switched_from_ktz\n");
}

static void switched_to_ktz(struct rq *rq, struct task_struct *p)
{
	LOG("Entering : switched_to_ktz\n");
	if (task_current(rq, p)) {
		/* We switched to this class while running. */
		LOG("Task was running when switching\n");
		start_runtime(p);
	}
}

static void prio_changed_ktz(struct rq*rq, struct task_struct *p, int oldprio)
{
	LOG("Entering : prio_changed_ktz\n");
}

static unsigned int get_rr_interval_ktz(struct rq* rq, struct task_struct *p)
{
	LOG("Entering : int get_rr_interval_ktz\n");
	return get_timeslice();
}
#ifdef CONFIG_SMP
/*
 * SMP related functions	
 */

static inline int select_task_rq_ktz(struct task_struct *p, int cpu, int sd_flags, int wake_flags)
{
	LOG("Entering : int select_task_rq_ktz\n");
	int new_cpu = smp_processor_id();
	
	return new_cpu; //set assigned CPU to zero
}


static void set_cpus_allowed_ktz(struct task_struct *p,  const struct cpumask *new_mask)
{
	LOG("Entering : set_cpus_allowed_ktz\n");
}
#endif
/*
 * Scheduling class
 */
static void update_curr_ktz(struct rq*rq)
{
	LOG("Entering : update_curr_ktz\n");
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
