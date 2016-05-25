/*
	Operating System 2016 project 3: Weighted Round-Robin scheduler
	Created by team1 - SUNG-YUN HUR, EUN-HYANG KIM, YEON-WOO KIM
*/

#include "sched.h"
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/rcupdate.h>
#define WRR_TIMESLICE (HZ / 100)
#define LB_INTERVAL (2 * HZ)

const struct sched_class wrr_sched_class;

static inline struct list_head *wrr_rq_list(struct wrr_rq *wrr_rq)
{
	return &wrr_rq->run_queue;
}

static inline bool is_wrr_rq_empty(struct wrr_rq *rq)
{
	return rq->run_queue.next == &rq->run_queue;
}

#define wrr_entity_is_task(wrr_se) (1)
static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
#ifdef CONFIG_SCHED_DEBUG
	WARN_ON_ONCE(!wrr_entity_is_task(wrr_se));
#endif
	return container_of(wrr_se, struct task_struct, wrr);
}

extern void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *rq)
{
	wrr_rq->total_weight = 0;
	INIT_LIST_HEAD(&wrr_rq->run_queue);
	wrr_rq->curr = NULL;
	raw_spin_lock_init(&wrr_rq->lock);
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct wrr_rq *wrr;
	struct sched_wrr_entity *se;
	struct list_head *se_list;
	struct list_head *rq_list;
	struct list_head *curr_list;
	struct sched_wrr_entity *curr_se;
	
	wrr = &rq->wrr;

	raw_spin_lock(&wrr->lock);

	se = &p->wrr;
	se_list = &se->run_list;
	rq_list = wrr_rq_list(wrr);


	if (wrr->curr == NULL) { /* < If the list is currently empty, set the cursor to the newly added task and add the task to the list */
		wrr->curr = p;
		list_add_tail(se_list, rq_list);
	}
	else { /* < If the list is not empty, simply add the task right before the cursor */
		curr_se = &wrr->curr->wrr;
		curr_list = &curr_se->run_list;
	
		list_add_tail(se_list, curr_list);
	}

	wrr->total_weight += se->weight;
	p->on_rq = 1;

	raw_spin_unlock(&wrr->lock);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct list_head *se_list;
	struct list_head *rq_list;
	struct wrr_rq *wrr;
	struct sched_wrr_entity *se;
	struct list_head* next_curr;

	wrr = &rq->wrr;

	raw_spin_lock(&wrr->lock);

	se = &p->wrr;
	se_list = &se->run_list;
	rq_list = wrr_rq_list(wrr);

	next_curr = se_list->next;

	list_del_init(se_list);

	if (is_wrr_rq_empty(wrr)) { /* < If the run queue is empty, set the cursor to null */
		wrr->curr = NULL;
	}
	else if (p == wrr->curr) { /* < Else if the deleting task is the task pointed by the cursor, update the cursor appropriately (considering the dummy head) */
		if (next_curr == rq_list) next_curr = next_curr->next;
		wrr->curr = wrr_task_of(list_entry(next_curr, struct sched_wrr_entity, run_list));
	}

	wrr->total_weight -= se->weight;
	p->on_rq = 0;

	raw_spin_unlock(&wrr->lock);
}

static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	return;	
}

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
	struct task_struct* curr = rq->wrr.curr;

	if (curr == NULL) return NULL;
	curr->wrr.time_slice = curr->wrr.weight * WRR_TIMESLICE;
	/* Return the task pointed by the cursor with updated timeslice */
	return curr;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	return;
}

static int find_lowest_rq(struct task_struct *p)
{
	int cpu;
	struct rq *rq;
	int best_cpu;
	unsigned long best_weight;
	struct wrr_rq *wrr;

	best_cpu = -1;

	for_each_online_cpu(cpu) {
		rq = cpu_rq(cpu);
		wrr = &rq->wrr;
		if((best_cpu == -1 || wrr->total_weight < best_weight) &&
				cpumask_test_cpu(cpu, tsk_cpus_allowed(p))) {
			best_cpu = cpu;
			best_weight = wrr->total_weight;
		}
	}
	return best_cpu;
}

static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	struct rq *rq;
	int cpu;
	int target;

	cpu = task_cpu(p);
	if (p->nr_cpus_allowed == 1) return cpu;

	rq = cpu_rq(cpu);

	rcu_read_lock();

	target = find_lowest_rq(p);
	if (target != -1) 
		cpu = target;
	rcu_read_unlock();

	return cpu;
}

static void set_curr_task_wrr(struct rq *rq)
{
}

static void update_curr(struct rq *rq)
{
	struct task_struct *curr;
	struct sched_wrr_entity *se;
  struct list_head *rq_list;
  struct list_head *se_list;
  struct list_head *next;
  struct wrr_rq *wrr_rq;

  wrr_rq = &rq->wrr;
  rq_list = wrr_rq_list(wrr_rq);

	if(rq->wrr.curr == NULL) return;
	curr = rq->wrr.curr;
	se = &curr->wrr;
	se_list = &se->run_list;
	
	/* Decrease the time slice of currently running task until it reaches zero */
	if (--se->time_slice) return;

	if(se_list->next != se_list->prev) { /* < If more than one element in the list, move the cursor to the next task and resched */
		next = se_list->next;
		if (next == &wrr_rq->run_queue) next = next->next;
		wrr_rq->curr = wrr_task_of(list_entry(next, struct sched_wrr_entity, run_list));
		set_tsk_need_resched(curr);
	}
	else se->time_slice = se->weight * WRR_TIMESLICE; /* < Else, refill the current task's time_slice */
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	update_curr(rq);	
}

static void task_fork_wrr(struct task_struct *p)
{
	/* child weight is the same as parent's */
	p->wrr.weight = p->real_parent->wrr.weight;
	p->wrr.time_slice = p->wrr.weight * WRR_TIMESLICE;
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{ 
	/* sched policy switched from other to wrr */
	p->wrr.weight = 10;
	p->wrr.time_slice = 10 * WRR_TIMESLICE;
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task) {
		return task->wrr.weight * WRR_TIMESLICE;
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

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {
}

static void switched_from_wrr(struct rq *rq, struct task_struct *p) {
}

static void yield_task_wrr(struct rq *rq) {
}

static bool yield_to_task_wrr(struct rq *rq, struct task_struct* p, bool preempt) {
	return true;
}

const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task = yield_task_wrr,
	.yield_to_task = yield_to_task_wrr,
	.check_preempt_curr	= check_preempt_curr_wrr,
	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
  .set_cpus_allowed    = set_cpus_allowed_wrr,
  .pre_schedule           = pre_schedule_wrr,
  .post_schedule          = post_schedule_wrr,
	.task_waking						= task_waking_wrr,
	.task_woken							= task_woken_wrr,
  .rq_online              = rq_online_wrr,
  .rq_offline             = rq_offline_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	.task_fork		= task_fork_wrr,

	.switched_to		= switched_to_wrr,
	.switched_from 	= switched_from_wrr,
	.prio_changed 	= prio_changed_wrr,
	.get_rr_interval	= get_rr_interval_wrr,
};
