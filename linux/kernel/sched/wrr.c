/*
	Implementation for weighted-round-robin scheduler.	
	TODO: Find where a lock is needed.
*/

#include "sched.h"
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/rcupdate.h>
#define WRR_TIMESLICE (10 * 1000000)
#define LB_INTERVAL (2000 * 1000000)

const struct sched_class wrr_sched_class;
u64 balance_timestamp;

static inline struct list_head *wrr_rq_list(struct wrr_rq *wrr_rq)
{
	return &wrr_rq->run_queue;
}

static inline bool is_wrr_rq_empty(struct wrr_rq *rq)
{
	return rq->run_queue.next == &rq->run_queue;
}

static void print_wrr_rq(struct rq *rq)
{
	struct sched_wrr_entity *se;
	struct task_struct *p;
	int count = 0;
	printk("sched_wrr: print runqueue[%d] running#: %d\n", rq->cpu, rq->wrr.nr_running);

	list_for_each_entry(se, wrr_rq_list(&rq->wrr), run_list) {
		p = container_of(se, struct task_struct, wrr);
		printk("\t\t\twrr_rq[%d][%d]: pid: %d, weight: %d, time_slice: %lld\n", 
				rq->cpu, count++, p->pid, p->wrr.weight, p->wrr.time_slice);
	}
}

extern void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *rq)
{
	printk("sched_wrr: init_wrr_rq\n");
	balance_timestamp = rq->clock;
	spin_lock_init(&wrr_rq->lock);
	wrr_rq->total_weight = 0;
	wrr_rq->nr_running = 0;
	INIT_LIST_HEAD(&wrr_rq->run_queue);
}

/* run queue management */

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct wrr_rq *wrr;
	struct sched_wrr_entity *se;
	struct list_head *se_list;
	struct list_head *rq_list;
	struct list_head *list;
	printk("sched_wrr: enqueue_task_wrr --- pid: %d\n", p->pid);
	
	se = &p->wrr;
	se_list = &se->run_list;
	wrr = &rq->wrr;
	rq_list = wrr_rq_list(wrr);

/*
	list_for_each(list, rq_list) {
		if (list == se_list) break;
		if (list->next == rq_list) {
			list_add_tail(se_list, rq_list);
			wrr->total_weight += se->weight;
			wrr->nr_running++;
		}
	}
	if (is_wrr_rq_empty(wrr)) {
		list_add_tail(se_list, rq_list);
		wrr->total_weight += se->weight;
		wrr->nr_running++;
	}
*/

	if (rq->curr == p) {
		list_add(se_list, rq_list);
		wrr->total_weight += se->weight;
		wrr->nr_running++;
	} else {
		list_add_tail(se_list, rq_list);
		wrr->total_weight += se->weight;
		wrr->nr_running++;
	}
	print_wrr_rq(rq);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct list_head *se_list;
	struct list_head *rq_list;
	struct list_head *list;
	struct list_head *next;
	struct wrr_rq *wrr;
	struct sched_wrr_entity *se;
	printk("sched_wrr: dequeue_task_wrr --- pid: %d\n", p->pid);

	se = &p->wrr;
	se_list = &se->run_list;
	wrr = &rq->wrr;
	rq_list = wrr_rq_list(&rq->wrr);
/*	
	list_for_each_safe(list, next, rq_list){
		if (list == se_list) {
			list_del(se_list);
			wrr->total_weight -= se->weight;
			wrr->nr_running--;
		}
	}
*/
	list_del(se_list);
	wrr->total_weight -= se->weight;
	wrr->nr_running--;

	print_wrr_rq(rq);
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p);
static void yield_task_wrr(struct rq *rq)
{
	printk("sched_wrr: yield_task_wrr --- rq[%d]-curr[%d]\n", rq->cpu, rq->curr->pid);

	put_prev_task_wrr(rq, rq->curr);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
//	printk("sched_wrr: check_preempt_curr_wrr --- rq[%d], p->pid[%d]\n", rq->cpu, p->pid);
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
	struct sched_wrr_entity *se;
	struct task_struct *ret;

	if (rq->curr->policy != SCHED_WRR) 
		return NULL;
	
	printk("sched_wrr: pick_next_task_wrr --- rq[%d]-curr[%d]-policy[%d]\n", rq->cpu, rq->curr->pid, rq->curr->policy);
	
	rq_list = wrr_rq_list(&rq->wrr);
	if (is_wrr_rq_empty(&rq->wrr))
		return NULL;
	se = list_entry(rq_list->next, struct sched_wrr_entity, run_list);
	ret = wrr_task_of(se);
	ret->wrr.exec_start = rq->clock;
	ret->wrr.time_slice = ret->wrr.weight * WRR_TIMESLICE;
	return ret;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	printk("sched_wrr: put_prev_task_wrr --- rq[%d]-curr[%d], p[%d]\n", rq->cpu, rq->curr->pid, p->pid);
	if (rq->curr == p && !is_wrr_rq_empty(&rq->wrr)) {
		printk("sched_wrr: put_prev_task_wrr --- entering dequeue\n");
		dequeue_task_wrr(rq, p, 0);
		printk("sched_wrr: put_prev_task_wrr --- entering enqueue\n");
		enqueue_task_wrr(rq, p, 0);
	}
}

/* CPU management */
static int find_lowest_rq(struct task_struct *p)
{
	int cpu;
	struct rq *rq;
	int best_cpu;
	unsigned long best_weight;
	struct wrr_rq *wrr;
	printk("sched_wrr: find_lowest_rq --- p[%d]\n", p->pid);

	if (p->nr_cpus_allowed == 1)
		return -1; /* No other targets possible */

	cpu = task_cpu(p);
	best_cpu = -1;
	best_weight = p->wrr.weight;

	for_each_online_cpu(cpu) {
		rq = cpu_rq(cpu);
		wrr = &rq->wrr;

		if(wrr->total_weight < best_weight) {
			best_cpu = cpu;
			best_weight = wrr->total_weight;
		}
	}
	return best_cpu;
}

static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	struct task_struct *curr;
	struct rq *rq;
	int cpu;
	int target;
	printk("sched_wrr: select_task_rq_wrr --- p->pid[%d]\n", p->pid);

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

static void migrate_task_rq_wrr(struct task_struct *p, int next_cpu)
{
	struct rq *rq;
	struct rq *next_rq;

	rq = cpu_rq(task_cpu(p));
	if (rq->curr == p) 
		return;
	next_rq = cpu_rq(next_cpu);
	
	printk("sched_wrr: migrate_task_rq_wrr --- entering dequeue\n");
	dequeue_task_wrr(rq, p, 0);
	printk("sched_wrr: migrate_task_rq_wrr --- entering enqueue\n");
	enqueue_task_wrr(next_rq, p, 0);
}

static void pre_schedule_wrr(struct rq *this_rq, struct task_struct *task)
{}
static void post_schedule_wrr(struct rq *this_rq)
{}
static void task_waking_wrr(struct task_struct *task)
{}
static void task_woken_wrr(struct rq *this_rq, struct task_struct *task)
{}

static void set_cpus_allowed_wrr(struct task_struct *p, const struct cpumask *newmask)
{}

static void rq_online_wrr(struct rq *rq)
{}
static void rq_offline_wrr(struct rq *rq)
{}

/* runtime management */

static void set_curr_task_wrr(struct rq *rq)
{

	struct task_struct *p;

	printk("sched_wrr: set_curr_task_wrr --- rq[%d]-curr[%d]-policy[%d]\n", rq->cpu, rq->curr->pid, rq->curr->policy);

	p = rq->curr;
	p->wrr.exec_start = rq->clock; /* load current time to exec_time */
	p->wrr.time_slice = p->wrr.weight * WRR_TIMESLICE;
}

static void update_curr_wrr(struct rq *rq)
{/* check curr task time_slice */
	struct task_struct *curr;
	struct sched_wrr_entity *se;
	u64 delta_exec;
	u64 now;
	printk("sched_wrr: update_curr_wrr --- rq[%d]-curr[%d]\n", rq->cpu, rq->curr->pid);

	now = rq->clock; 
	curr = rq->curr;
	se = &curr->wrr;
	delta_exec = rq->clock - se->exec_start;

	printk("\t\t\texec_start: %lld, time_slice: %lld, now: %lld\n", se->exec_start, se->time_slice, now);
	if (time_before(se->exec_start + se->time_slice, now))
		return;

	resched_task(curr);
}

static int is_migratable(struct rq *rq, struct task_struct *p) {
	if (p->nr_cpus_allowed == 1 || rq->curr == p) {
		return 0;
	} else {
		return 1;
	}
}

static void load_balance(struct rq *rq){
	int cpu;
	unsigned int max_weight = rq->wrr.total_weight;
	unsigned int min_weight = rq->wrr.total_weight;
	struct rq *min_rq = rq;
	struct rq *max_rq = rq;
	struct wrr_rq *wrr;
	struct list_head* list;
	struct sched_wrr_entity *se;
	struct task_struct *mp; /* migrating task */
	unsigned int mweight;
	struct task_struct *p;
	printk("sched_wrr: load_balance --- rq[%d]-curr[%d]\n", rq->cpu, rq->curr->pid);

	balance_timestamp = rq->clock;
	rcu_read_lock();
	for_each_online_cpu(cpu) {
		rq = cpu_rq(cpu);
		wrr = &rq->wrr;
		//spin_lock(&weight_lock);
		if (wrr->total_weight < min_weight) {
			min_rq = rq;
			min_weight = wrr->total_weight;
		}
		if (wrr->total_weight > max_weight) {
			max_rq = rq;
			max_weight = wrr->total_weight;
		}
		//spin_unlock(&weight_lock);
	}
	rcu_read_unlock();

	if (min_rq == max_rq) return;

	/*needs synchronization; task in different cpu might access another cpu's rq*/

	mweight = 0;
	list = wrr_rq_list(&max_rq->wrr);

	list_for_each_entry(se, list, run_list) {
		p = wrr_task_of(se);
		//spin_lock(&weight_lock);
		if (is_migratable(max_rq, p) &&
				se->weight > mweight &&
				min_weight + se->weight < max_weight - se->weight) {
			mp = p;
			mweight = se->weight;
		}
		//spin_unlock(&weight_lock);
	}

	if (mp == NULL) return;
	
	migrate_task_rq_wrr(mp, min_rq->cpu);
//	dequeue_task_wrr(max_rq, mp, 0);
//	enqueue_task_wrr(min_rq, mp, 0);

}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	printk("sched_wrr: task_tick_wrr --- rq[%d], p->pid[%d]\n", rq->cpu, p->pid);
	/* update current running task */
	update_curr_wrr(rq);	
	/* load balancing */
	if (rq->clock - balance_timestamp == LB_INTERVAL) {
		load_balance(rq);
	}
}

static void task_fork_wrr(struct task_struct *p)
{/* child weight is the same as parent's */
	int cpu;
	struct rq *rq;

	printk("sched_wrr: task_fork_wrr --- p->pid[%d]\n", p->pid);
	p->wrr.weight = p->real_parent->wrr.weight;
	cpu = select_task_rq_wrr(p, 0, 0);
	rq = cpu_rq(cpu);
	printk("sched_wrr: task_fork_wrr --- entering enqueue\n");
	enqueue_task_wrr(rq, p, 0);
}

static void switched_from_wrr(struct rq *rq, struct task_struct *p)
{
	printk("sched_wrr: switched_from_wrr --- rq[%d], p->pid[%d]\n", rq->cpu, p->pid);
	return;
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{/* sched policy switched from other to wrr */
	printk("sched_wrr: switched_to_wrr --- rq[%d], p->pid[%d]\n", rq->cpu, p->pid);
	
	p->wrr.weight = 10;
	p->wrr.time_slice = 10 * WRR_TIMESLICE;
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
	if (task->policy == SCHED_WRR) {
		return task->wrr.time_slice;
	} else {
		return -EINVAL;
	}
}

const struct sched_class wrr_sched_class = {
/* TODO: delete functions we don't need in this project */
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,							//o	o
	.dequeue_task		= dequeue_task_wrr,							//o	o
	.yield_task		= yield_task_wrr,									//o	o
//	.yield_to_task		= yield_to_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,		//o	o

	.pick_next_task		= pick_next_task_wrr,					//o	o
	.put_prev_task		= put_prev_task_wrr,					//o	o

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,					//o
	.set_cpus_allowed    = set_cpus_allowed_wrr,
	.rq_online		= rq_online_wrr,
	.rq_offline		= rq_offline_wrr,
	.pre_schedule		= pre_schedule_wrr,
	.post_schedule		= post_schedule_wrr,

#endif

	.set_curr_task          = set_curr_task_wrr,		//o	o
	.task_tick		= task_tick_wrr,									//o
	.task_fork		= task_fork_wrr,									//o

	.switched_from		= switched_from_wrr,					//o
	.switched_to		= switched_to_wrr,							//o

	.get_rr_interval	= get_rr_interval_wrr,				//o
};
