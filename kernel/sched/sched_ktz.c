#include "sched.h"

/* Macros and defines. */
/* Timeshare range = Whole range of this scheduler. */
#define	PRI_TIMESHARE_RANGE	(PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1)
#define PRI_MIN_TIMESHARE	MIN_KTZ_PRIO
#define PRI_MAX_TIMESHARE	MAX_KTZ_PRIO

/* Interactive range. */
#define	PRI_INTERACT_RANGE	5
#define	PRI_MIN_INTERACT	PRI_MIN_TIMESHARE
#define	PRI_MAX_INTERACT	(PRI_MIN_TIMESHARE + PRI_INTERACT_RANGE - 1)

/* Batch range. */
#define	PRI_BATCH_RANGE		(PRI_TIMESHARE_RANGE - PRI_INTERACT_RANGE)
#define	PRI_MIN_BATCH		(PRI_MIN_TIMESHARE + PRI_INTERACT_RANGE)
#define	PRI_MAX_BATCH		PRI_MAX_TIMESHARE

/* Batch range separation. */ /* TODO : Hardcoded */
#define	SCHED_PRI_NRESV		(PRIO_MAX - PRIO_MIN)
#define	SCHED_PRI_NHALF		(SCHED_PRI_NRESV / 2)
#define	SCHED_PRI_MIN		133
#define	SCHED_PRI_MAX		136
#define	SCHED_PRI_RANGE		(SCHED_PRI_MAX - SCHED_PRI_MIN + 1)

/* Macros/defines used for stat computation. */
#define	SCHED_TICK_SHIFT	10	/* Used to avoid rounding errors. */
#define	SCHED_TICK_SECS		10	/* Number of secs for cpu stats. */
#define	SCHED_TICK_TARG		(HZ * SCHED_TICK_SECS)	/* 10s in ticks. */
#define	SCHED_TICK_MAX		(SCHED_TICK_TARG + HZ)
#define	SCHED_SLP_RUN_MAX	((HZ * 5) << SCHED_TICK_SHIFT)
#define	SCHED_INTERACT_MAX	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_MAX / 2)
#define	SCHED_INTERACT_THRESH	(30)

/* Globals */
static int tickincr = 1 << SCHED_TICK_SHIFT;	/* 1 Should be correct. */
static int sched_interact = SCHED_INTERACT_THRESH;

/* Helper macros / defines. */
#define LOG(...) 	printk_deferred(__VA_ARGS__)
#define KTZ_SE(p)	(&(p)->ktz_se)
#define PRINT(name)	printk_deferred(#name "\t\t = %d", name)

void init_ktz_tdq(struct ktz_tdq *ktz_tdq)
{
	INIT_LIST_HEAD(&ktz_tdq->queue);

	/* Print config. */
	PRINT(tickincr);
}

static void pctcpu_update(struct sched_ktz_entity *ts, bool run)
{
	int t = jiffies;

	if ((uint)(t - ts->ltick) >= SCHED_TICK_TARG) {
		ts->ticks = 0;
		ts->ftick = t - SCHED_TICK_TARG;
	}
	else if (t - ts->ftick >= SCHED_TICK_MAX) {
		ts->ticks = (ts->ticks / (ts->ltick - ts->ftick)) *
		    (ts->ltick - (t - SCHED_TICK_TARG));
		ts->ftick = t - SCHED_TICK_TARG;
	}
	if (run)
		ts->ticks += (t - ts->ltick) << SCHED_TICK_SHIFT;
	ts->ltick = t;
}

/*
 * This routine enforces a maximum limit on the amount of scheduling history
 * kept.  It is called after either the slptime or runtime is adjusted.  This
 * function is ugly due to integer math.
 */
static void interact_update(struct task_struct *p)
{
	u_int sum;
	struct sched_ktz_entity *ke_se = KTZ_SE(p);

	sum = ke_se->runtime + ke_se->slptime;
	if (sum < SCHED_SLP_RUN_MAX)
		return;
	/*
	 * This only happens from two places:
	 * 1) We have added an unusual amount of run time from fork_exit.
	 * 2) We have added an unusual amount of sleep time from sched_sleep().
	 */
	if (sum > SCHED_SLP_RUN_MAX * 2) {
		if (ke_se->runtime > ke_se->slptime) {
			ke_se->runtime = SCHED_SLP_RUN_MAX;
			ke_se->slptime = 1;
		} else {
			ke_se->slptime = SCHED_SLP_RUN_MAX;
			ke_se->runtime = 1;
		}
		return;
	}
	/*
	 * If we have exceeded by more than 1/5th then the algorithm below
	 * will not bring us back into range.  Dividing by two here forces
	 * us into the range of [4/5 * SCHED_INTERACT_MAX, SCHED_INTERACT_MAX]
	 */
	if (sum > (SCHED_SLP_RUN_MAX / 5) * 6) {
		ke_se->runtime /= 2;
		ke_se->slptime /= 2;
		return;
	}
	ke_se->runtime = (ke_se->runtime / 5) * 4;
	ke_se->slptime = (ke_se->slptime / 5) * 4;
}

static int interact_score(struct task_struct *p)
{
	int div;
	struct sched_ktz_entity *ktz_se = KTZ_SE(p);

	/*
	 * The score is only needed if this is likely to be an interactive
	 * task.  Don't go through the expense of computing it if there's
	 * no chance.
	 */
	if (sched_interact <= SCHED_INTERACT_HALF &&
		ktz_se->runtime >= ktz_se->slptime)
			return (SCHED_INTERACT_HALF);

	if (ktz_se->runtime > ktz_se->slptime) {
		div = max(1, ktz_se->runtime / SCHED_INTERACT_HALF);
		return (SCHED_INTERACT_HALF +
		    (SCHED_INTERACT_HALF - (ktz_se->slptime / div)));
	}
	if (ktz_se->slptime > ktz_se->runtime) {
		div = max(1, ktz_se->slptime / SCHED_INTERACT_HALF);
		return (ktz_se->runtime / div);
	}
	/* runtime == slptime */
	if (ktz_se->runtime)
		return (SCHED_INTERACT_HALF);

	/*
	 * This can happen if slptime and runtime are 0.
	 */
	return (0);

}

static inline void print_stats(struct task_struct *p)
{
	struct sched_ktz_entity *kse = KTZ_SE(p);
	unsigned long long st = kse->slptime >> SCHED_TICK_SHIFT;
	unsigned long long rt = kse->runtime >> SCHED_TICK_SHIFT;
	int interact = interact_score(p);
	LOG("Task %d : ", p->pid);
	LOG("\t| slptime\t\t= %llu ms", st);
	LOG("\t| runtime\t\t= %llu ms", rt);
	LOG("\t| interact\t\t= %d", interact);
}

static inline struct task_struct *ktz_task_of(struct sched_ktz_entity *ktz_se)
{
	return container_of(ktz_se, struct task_struct, ktz_se);
}

static void enqueue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_ktz_entity *ktz_se = KTZ_SE(p);
	struct list_head *queue = &rq->ktz.queue;

	add_nr_running(rq,1);
	if (flags & ENQUEUE_WAKEUP) {
		LOG("Task %d is waking up\n", p->pid);
		/* Count sleeping ticks. */
		ktz_se->slptime += (jiffies - ktz_se->slptick) << SCHED_TICK_SHIFT;
		ktz_se->slptick = 0;
		interact_update(p);
		pctcpu_update(ktz_se, false);
	}
	ktz_se->slice = 0;
	list_add_tail(&ktz_se->run_list, queue);
}

static void dequeue_task_ktz(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_ktz_entity *ktz_se = &p->ktz_se;

	sub_nr_running(rq,1);
	if (flags & DEQUEUE_SLEEP) {
		LOG("Task %d is going to sleep\n", p->pid);
		ktz_se->slptick = jiffies;
	}
	list_del_init(&ktz_se->run_list);
	print_stats(p);
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
	struct sched_ktz_entity *ktz_se = KTZ_SE(curr);

	/* Account runtime. */
	ktz_se->runtime += tickincr;
	interact_update(curr);

	/* Update CPU stats. */
	pctcpu_update(ktz_se, true);
}

static void task_fork_ktz(struct task_struct *p)
{
}

static void task_dead_ktz(struct task_struct *p)
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
	return 0;
}
#ifdef CONFIG_SMP
static inline int select_task_rq_ktz(struct task_struct *p, int cpu, int sd_flags, int wake_flags)
{
	int new_cpu = smp_processor_id();
	
	return new_cpu; //set assigned CPU to zero
}


static void set_cpus_allowed_ktz(struct task_struct *p,  const struct cpumask *new_mask)
{
}
#endif

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
	.task_fork		= task_fork_ktz,
	.task_dead		= task_dead_ktz,

	.switched_from		= switched_from_ktz,
	.switched_to		= switched_to_ktz,
	.prio_changed		= prio_changed_ktz,

	.get_rr_interval	= get_rr_interval_ktz,
	.update_curr		= update_curr_ktz,
};

