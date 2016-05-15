/*
	Implementation for weighted-round-robin scheduler.	
	TODO: Find where a lock is needed.
*/

#include "sched.h"
#include <linux/slab.h>
#define WRR_TIMESLICE (10 * HZ / 1000)

const struct sched_class wrr_sched_class;

void init_wrr_rq(struct wrr_rq *wrr_rq)
{

#if defined CONFIG_SMP
#endif

	wrr_rq->total_weight = 0;
	wrr_rq->nr_running = 0;
	wrr_rq->run_queue; /* new sched_wrr_entity */
	wrr_rq->lb_interval = (2000 * HZ / 1000); /* 2000ms */
}


/* run queue management */
static inline void list_head *wrr_rq_list(struct wrr_rq *wrr_rq)
{
	return &wrr_rq->run_queue.run_list;
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p)
{
	struct sched_wrr_entity *se;
	struct wrr_rq *wrr;
	struct list_head *se_list;
	struct list_head *rq_list;
	
	wrr = &rq->wrr;
	se = &p->wrr;
	se_list = &se->run_list;
	rq_list = wrr_rq_list(wrr);
	
	list_add_tail(se_list, rq_list);
	wrr->total_weight += se->weight;
	wrr->nr_running++;
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p)
{
	struct sched_wrr_entity *se;
	struct list_head *se_list;
	struct list_head *rq_list;
	struct wrr_rq *wrr;
	
	wrr = &rq->wrr;
	se = &p->wrr;
	se_list = &se->run_list;
	rq_list = wrr_rq_list(wrr);
	
	list_del_init(se_list);
	wrr->total_weight -= se->weight;
	wrr->nr_running--;
}

static void yield_task_wrr(struct rq *rq)
{
	struct list_head *curr;
	struct wrr_rq *wrr;
	struct list_head *rq_list;

	wrr = &rq->wrr;
	rq_list = wrr_rq_list(wrr);
	curr = &(rq->curr->wrr.run_list);
	list_del_init(curr);
	list_add_tail(curr, rq_list);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p)
{
	return;	
}

#define wrr_entity_is_task(wrr_se) (1)
static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
#ifdef CONFIG_SCHED_DEBUG
/* no such bug */
	WARN_ON_ONCE(!wrr_entity_is_task(wrr_se));
#endif
	return container_of(wrr_se, struct task_struct, wrr);
}

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{// first task in the run queue
	struct list_head *rq_list;
	struct wrr_rq *wrr;
	struct sched_wrr_entity *se;
	struct task_struct *ret;

	wrr = &rq->wrr;
	rq_list = wrr_rq_list(wrr);
	se = list_entry(rq_list->next, struct sched_wrr_entity, run_list);
	ret = wrr_task_of(se); 

	rq->curr = ret;
	set_curr_task_wrr(rq);

	return ret;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	dequeue_task_wrr(rq, p);
	enqueue_task_wrr(rq, p);
}

static int find_lowest_rq(struct task_struct *p)
{
	int cpu;
	struct rq *rq;
	int best_cpu;
	unsigned long best_weight;
	struct wrr_rq *wrr;

	if (p->nr_cpus_allowed == 1)
		return -1; /* No other targets possible */

	cpu = task_cpu(p);
	best_cpu = -1;
	best_weight = p->wrr.weight;

	for_each_possible(cpu) {
		rq = cpu_rq(cpu);
		wrr = &rq->wrr;

		if(wrr->total_weight < best_weight) {
			best_cpu = cpu;
			best_weight = wrr->total_weight;
		}
	}
	return best_cpu;
}

static int select_task_rq_wrr(struct task_struct *p)
{
	struct task_struct *curr;
	struct rq *rq;
	int cpu;
	int target;

	cpu = task_cpu(p);
	if (p->nr_cpus_allowed == 1) return cpu;

	rq = cpu_rq(cpu);

	rcu_read_lock();
	curr = ACCESS_ONCE(rq->curr);
	if (curr == p) /* if the task is currently running */
		/*TODO: ERROR*/;

	target = find_lowest_rq(p);
	if (target != -1) 
		cpu = target;
	rcu_read_unlock();

	return cpu;
}

/*
	TODO: CPU things 
*/

/* runtime management */
static void set_curr_task_wrr(struct rq *rq)
{
	struct task_struct *p;

	p = rq->curr;
	p->wrr.exec_start = rq->clock_task; /* load current time to exec_time */
	p->wrr.time_slice = p->wrr.weight * WRR_TIMESLICE;
}

static void update_curr_wrr(struct rq *rq)
{/* check curr task time_slice */
	struct task_struct *curr;
	struct sched_wrr_entity *se;
	u64 delta_exec;

	curr = rq->curr;
	se = &curr->wrr;
	delta_exec = rq->clock_task - se->exec_start;

	if (time_before(se->exec_start + se->time_slice, now))
		return;

	yield_task_wrr(rq);
	pick_next_task_wrr(rq);
}

static int is_migratable(struct rq *rq, struct task_struct *p) {
	int cpu;
	cpu = task_cpu(p);

	if (p->nr_cpus_allowed == 1 || rq->curr == p) return 0;
	else return 1;
}

static void load_balance(struct rq *rq){
	int cpu;
	unsigned int max_weight = 0;
	unsigned int min_weight = INT_MAX;
	struct rq *min_rq;
	struct rq *max_rq;
	struct rq* rq;
	struct wrr_rq *wrr;
	struct list_head* list;
	struct sched_wrr_entity *se;
	struct task_struct *mp; /* migrating task */
	unsigned int mweight;
	struct task_struct *p;

	rcu_readlock();
	for_each_possible(cpu) {
		rq = cpu_rq(cpu);
		wrr = &rq->wrr;
		if (wrr->total_weight < min_weight) {
			min_rq = rq;
			min_weight = wrr->total_weight;
		}
		if (wrr->total_weight > max_weight) {
			max_rq = rq;
			max_weight = wrr->total_weight;
		}
	}
	rcu_readunlock();

	if (min_rq == max_rq) return;

	/*needs synchronization; task in different cpu might access another cpu's rq*/

	mweight = 0;
	list = wrr_rq_list(&max_rq->wrr);
	list_for_each_entry(se, list, run_list) {
		p = wrr_task_of(se);
		if (is_migratable(max_rq, p) &&
				se->weight > mweight &&
				min_weight + se->weight < max_weight - se->weight) {
			mp = p;
			mweight = se->weight;
		}
	}

	if (mse == NULL) return;
	dequeue_task_wrr(max_rq, mp);
	enqueue_task_wrr(min_rq, mp);
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p)
{
	struct wrr_sched_entity *se = &p->wrr;
	/* update current running task */
	update_curr_wrr(rq);	
	/* load balancing */
	load_balance(rq);
}

static void task_fork_wrr(struct task_struct *p)
{/* child weight is the same as parent's */
	int cpu;
	struct rq *rq;

	p->wrr.weight = p->real_parent->wrr.weight;
	cpu = select_task_rq_wrr(p);
	rq = cpu_rq(cpu);
	enqueue_task_wrr(rq, p);
}

static void switched_from_wrr(struct rq *rq, struct task_struct *p)
{
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{/* sched policy switched from other to wrr */
	p->wrr.weight = 10;
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
	if (task->policy == SCHED_WRR)
		return task->wrr.timeslice;
	else
		return 0;
}

const struct sched_class wrr_sched_class = {
/* TODO: delete functions we don't need in this project */
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,							//o
	.dequeue_task		= dequeue_task_wrr,							//o
	.yield_task		= yield_task_wrr,									//o
//	.yield_to_task		= yield_to_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,		//o

	.pick_next_task		= pick_next_task_wrr,					//o
	.put_prev_task		= put_prev_task_wrr,					//o

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,					//o
	.set_cpus_allowed       = set_cpus_allowed_wrr,
	.rq_online		= rq_online_wrr,
	.rq_offline		= rq_offline_wrr,
	.pre_schedule		= pre_schedule_wrr,
	.post_schedule		= post_schedule_wrr,
	.switched_from		= switched_from_wrr,

#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	.task_fork		= task_fork_wrr,

	.switched_to		= switched_to_wrr,

	.get_rr_interval	= get_rr_interval_wrr,
};
