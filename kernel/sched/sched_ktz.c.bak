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

#define TRACE(...)

#define KTZ_TIMESLICE		(100 * HZ / 1000)
#define KTZ_AGE_THRESHOLD	(3 * KTZ_TIMESLICE)

#define TDQ_SELF(rq) \
	&rq->ktz

#define SCHED_TICK_SHIFT 10 /* 10 */
#define	SCHED_INTERACT_THRESH	(30)
#define	SCHED_INTERACT_MAX	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_MAX / 2)



/* To avoid rounding errors. */
static int tickincr = 8 << SCHED_TICK_SHIFT; /* 8 */
static int sched_interact = SCHED_INTERACT_THRESH;

static struct sched_ktz_entity *ktz_se_of_task(struct task_struct *p)
{
	return &p->ktz_se;
}

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

void print_def_bsd(void);

void init_ktz_tdq(struct ktz_tdq *ktz_tdq)
{
	int i;
	struct runq *q;

	print_def_bsd();
	printk_deferred("KTZ Scheduler initialized\n");
	printk_deferred("KTZ_HEADS_PER_RUNQ	= %d\n", KTZ_HEADS_PER_RUNQ);
	printk_deferred("KTZ_RUNQ_BITMAP_SIZE	= %ld\n", KTZ_RUNQ_BITMAP_SIZE);
	printk_deferred("task_struct size	= %ld\n", sizeof(struct task_struct));
	INIT_LIST_HEAD(&ktz_tdq->queue);

	q = &ktz_tdq->timeshare;
	for (i = 0; i < KTZ_HEADS_PER_RUNQ; i ++) {
		INIT_LIST_HEAD(&q->queues[i]);
	}
}

static void print_times(const char *prefix, struct task_struct *p)
{
	struct sched_ktz_entity ke = *ktz_se_of_task(p);
	unsigned long long slp = ke.slptime;
	unsigned long long run = ke.runtime;
	printk_deferred("%s : Task %d, slptime = %llu, runtime = %llu\n", prefix, p->pid, slp, run);
}

/*
 * Helper functions
 */

static inline struct task_struct *ktz_task_of(struct sched_ktz_entity *ktz_se)
{
	return container_of(ktz_se, struct task_struct, ktz_se);
}

static void tdq_add(struct ktz_tdq *tdq, struct task_struct *p, int flags)
{
	int pri;
	struct runq *dst_runq;
	struct sched_ktz_entity ke = *ktz_se_of_task(p);

	pri = p->prio;
	LOG("prio = %d", pri);
	if (pri < tdq->lowpri)
		tdq->lowpri = pri;

	dst_runq = &tdq->timeshare;
	if ((flags & (KTZ_PREEMPTED)) == 0) {
		pri = KTZ_HEADS_PER_RUNQ * (pri - MIN_KTZ_PRIO) / KTZ_PRIO_RANGE;
		LOG("q0 : %d", pri);
		pri = (pri + tdq->idx) % KTZ_HEADS_PER_RUNQ;
		LOG("q : %d", pri);

		if (tdq->ridx != tdq->idx && pri == tdq->ridx) {
			LOG("Condition true");
			pri = (int)(pri - 1) % KTZ_HEADS_PER_RUNQ;
		}
	}
	else {
		/* If preempted add to head. */
		pri = tdq->ridx;
	}

	/* For now timeshare only. But may not be the case in the future. */
	LOG("Add task %d into runq %d", p->pid, pri);
	runq_add_pri(dst_runq, p, pri, flags);
}

static int sched_interact_score(struct task_struct *p)
{
	int div;
	struct sched_ktz_entity *ke = ktz_se_of_task(p);

	/*
	 * The score is only needed if this is likely to be an interactive
	 * task.  Don't go through the expense of computing it if there's
	 * no chance.
	 */
	if (sched_interact <= SCHED_INTERACT_HALF && ke->runtime >= ke->slptime)
		return (SCHED_INTERACT_HALF);

	if (ke->runtime > ke->slptime) {
		div = max(1, ke->runtime / SCHED_INTERACT_HALF);
		return (SCHED_INTERACT_HALF + (SCHED_INTERACT_HALF - (ke->slptime / div)));
	}
	if (ke->slptime > ke->runtime) {
		div = max(1, ke->slptime / SCHED_INTERACT_HALF);
		return (ke->runtime / div);
	}
	/* runtime == slptime */
	if (ke->runtime)
		return (SCHED_INTERACT_HALF);

	/*
	 * This can happen if slptime and runtime are 0.
	 */
	return (0);

}

static void sched_priority(struct task_struct *p)
{
	int score;
	int pri;

	/*
	 * If the score is interactive we place the thread in the realtime
	 * queue with a priority that is less than kernel and interrupt
	 * priorities.  These threads are not subject to nice restrictions.
	 *
	 * Scores greater than this are placed on the normal timeshare queue
	 * where the priority is partially decided by the most recent cpu
	 * utilization and the rest is decided by nice value.
	 *
	 * The nice value of the process has a linear effect on the calculated
	 * score.  Negative nice values make it easier for a thread to be
	 * considered interactive.
	 */
	score = max(0, sched_interact_score(p) + task_nice(p));
	if (score < sched_interact) {
		//pri = PRI_MIN_INTERACT;
		//pri += ((KTZ_MAX_INTERACT - KTZ_MIN_INTERACT) / KTZ_INTERACT_RANGE) * score;
	} else {
		//pri = PRI_MIN_BATCH;
		/*if (td_get_sched(td)->ts_ticks)
			pri += min(SCHED_PRI_TICKS(td_get_sched(td)), SCHED_PRI_RANGE - 1);*/
		pri += task_nice(p);
	}
	LOG("Priority for %d goes to : %d", p->pid, pri);
	p->prio = pri;
}

/*
 * Scheduling class functions to implement
 */

static void enqueue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	TRACE("Entering : enqueue_task_ktz\n");
	struct sched_ktz_entity *ktz_se = ktz_se_of_task(p);
	struct list_head *queue = &rq->ktz.queue;
	struct ktz_tdq *tdq;

	add_nr_running(rq,1);
	if (flags & ENQUEUE_WAKEUP) {
		LOG("Task %d is waking up\n", p->pid);
		ktz_se->slptime += (jiffies - ktz_se->startslp) << SCHED_TICK_SHIFT;
	}

	sched_priority(p);

	list_add_tail(&ktz_se->run_list, queue);

	tdq = &rq->ktz;
	tdq_add(tdq, p, 0);
}

static void dequeue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	TRACE("Entering : dequeue_task_ktz\n");
	struct sched_ktz_entity *ktz_se = ktz_se_of_task(p);

	if (flags & DEQUEUE_SLEEP) {
		LOG("Task %d is going to sleep\n", p->pid);
		ktz_se->startslp = jiffies;
	}
	sub_nr_running(rq,1);
	list_del_init(&ktz_se->run_list);
	print_times("Dequeue", p);
}

static void yield_task_ktz(struct rq *rq)
{
	TRACE("Entering : yield_task_ktz\n");
}

static void check_preempt_curr_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	TRACE("Entering : check_preempt_curr_ktz\n");
}

static struct task_struct *pick_next_task_ktz(struct rq *rq, struct task_struct* prev, struct rq_flags *flags)
{
	TRACE("Entering : task_struct *pick_next_task_ktz\n");
	struct ktz_tdq *ktz_tdq = &rq->ktz;
	struct sched_ktz_entity *next;
	struct task_struct *next_task;
	put_prev_task(rq, prev);
	if(!list_empty(&ktz_tdq->queue)) {
		next = list_first_entry(&ktz_tdq->queue, struct sched_ktz_entity, run_list);
		next_task = ktz_task_of(next);
		return next_task;
	}
	else {
		return NULL;
	}
}

static void put_prev_task_ktz(struct rq *rq, struct task_struct *prev)
{
	TRACE("Entering : put_prev_task_ktz\n");
}

static void set_curr_task_ktz(struct rq *rq)
{
	TRACE("Entering : set_curr_task_ktz\n");
}

static void task_tick_ktz(struct rq *rq, struct task_struct *curr, int queued)
{
	TRACE("Entering : task_tick_ktz\n");
	struct sched_ktz_entity *ke = ktz_se_of_task(curr);
	ke->runtime += tickincr;
}

void task_fork_ktz(struct task_struct *p)
{
	TRACE("Entering : task_fork_ktz");
}

void task_dead_ktz(struct task_struct *p)
{
	TRACE("Entering : task_dead_ktz");
}

static void switched_from_ktz(struct rq *rq, struct task_struct *p)
{
	TRACE("Entering : switched_from_ktz\n");
}

static void switched_to_ktz(struct rq *rq, struct task_struct *p)
{
	TRACE("Entering : switched_to_ktz\n");
	if (task_current(rq, p)) {
		/* We switched to this class while running. */
		LOG("Task was running when switching\n");
	}
}

static void prio_changed_ktz(struct rq*rq, struct task_struct *p, int oldprio)
{
	TRACE("Entering : prio_changed_ktz\n");
}

static unsigned int get_rr_interval_ktz(struct rq* rq, struct task_struct *p)
{
	TRACE("Entering : int get_rr_interval_ktz\n");
	return get_timeslice();
}
#ifdef CONFIG_SMP
/*
 * SMP related functions	
 */

static inline int select_task_rq_ktz(struct task_struct *p, int cpu, int sd_flags, int wake_flags)
{
	TRACE("Entering : int select_task_rq_ktz\n");
	int new_cpu = smp_processor_id();
	return new_cpu; //set assigned CPU to zero
}


static void set_cpus_allowed_ktz(struct task_struct *p,  const struct cpumask *new_mask)
{
	TRACE("Entering : set_cpus_allowed_ktz\n");
}
#endif
/*
 * Scheduling class
 */
static void update_curr_ktz(struct rq*rq)
{
	TRACE("Entering : update_curr_ktz\n");
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
	.task_fork		= task_fork_ktz,
	.task_dead		= task_dead_ktz,

	.switched_from		= switched_from_ktz,
	.switched_to		= switched_to_ktz,
	.prio_changed		= prio_changed_ktz,

	.get_rr_interval	= get_rr_interval_ktz,
	.update_curr		= update_curr_ktz,
};

void print_def_bsd(void)
{
	LOG("PRI_TIMESHARE_RANGE = %d\n", PRI_TIMESHARE_RANGE);
	LOG("PRI_INTERACT_RANGE  = %d\n", PRI_INTERACT_RANGE );
	LOG("PRI_BATCH_RANGE	 = %d\n", PRI_BATCH_RANGE);
	LOG("PRI_MIN_INTERACT    = %d\n", PRI_MIN_INTERACT);
	LOG("PRI_MAX_INTERACT    = %d\n", PRI_MAX_INTERACT);
	LOG("PRI_MIN_BATCH	 = %d\n", PRI_MIN_BATCH);
	LOG("PRI_MAX_BATCH	 = %d\n", PRI_MAX_BATCH);
	LOG("SCHED_PRI_NRESV	 = %d\n", SCHED_PRI_NRESV);
	LOG("SCHED_PRI_NHALF	 = %d\n", SCHED_PRI_NHALF);
	LOG("SCHED_PRI_MIN	 = %d\n", SCHED_PRI_MIN);
	LOG("SCHED_PRI_MAX	 = %d\n", SCHED_PRI_MAX);
	LOG("SCHED_PRI_RANGE	 = %d\n", SCHED_PRI_RANGE);
}
