/*
 * Run queue structure.  Contains an array of run queues on which processes
 * are placed, and a structure to maintain the status of each queue.
 */
#define KTZ_HEADS_PER_RUNQ (64)
#define KTZ_RUNQ_BITMAP_SIZE KTZ_HEADS_PER_RUNQ / (sizeof(unsigned long) * 8)

struct runq {
	DECLARE_BITMAP(status, KTZ_RUNQ_BITMAP_SIZE); 
	struct list_head queues[KTZ_HEADS_PER_RUNQ];
};
