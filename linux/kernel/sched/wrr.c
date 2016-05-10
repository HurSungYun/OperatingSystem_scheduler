/*
	Implementation for weighted-round-robin scheduler.	
	TODO: Find where a lock is needed.
*/

#include "sched.h"
#include <linux/slab.h>

const struct sched_class wrr_sched_class;

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
	
	list_del_init(se_list, rq_list);
	wrr->total_weight -= se->weight;
}

static void yield_task_wrr(struct rq *rq)
{
	struct list_head *curr;
	struct wrr_rq *wrr;
	struct list_head *rq_list;

	wrr = &rq->wrr;
	rq_list = wrr_rq_list(wrr);
	curr = rq->curr->wrr.run_list;
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

	rq_list = wrr_rq_list(wrr);
	se = list_entry(rq_list->next, struct sched_wrr_entity, run_list);
	if (se == NULL) 
		/*TODO: error*/;
	ret = wrr_task_of(se); 
	if (ret == NULL)
		/*TODO: error*/;

	rq->curr = ret;
	return ret;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
	dequeue_task_wrr(rq, p);
	enqueue_task_wrr(rq, p);
}

static int find_lowest_rq(struct task_struct *p)
{
	/*TODO: compare total weights of run queues and pick the smallest one */
	return 0;
}
static int select_task_rq_wrr(struct task_struct *p)
{
	struct task_struct *curr;
	struct rq *rq;
	int cpu;
	int target;

	cpu = task_cpu(p);
	if (p->nr_cpus_allowed == 1) 
		goto out;
	/*TODO: check other things */

	rq = cpu_rq(cpu);

	rcu_read_lock();
	curr = ACCESS_ONCE(rq->curr);

	/*TODO: check if the task is running */
	target = find_lowest_rq(p); //TODO: implement find_lowest_rq
	if (target != -1) 
		cpu = target;
	rcu_read_unlock();

out:
	return cpu;
}

/*
	TODO: CPU things 
*/

/* runtime management */
static void set_curr_task_rt(struct rq *rq)
{
	struct task_struct *p;

	p = rq->curr;
	p->wrr.exec_start = rq->clock_task;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p)
{
	struct wrr_sched_entity *se = &p->rt;

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
	.select_task_rq		= select_task_rq_wrr,					//TODO: find_lowest_rq()
/*
#ifdef CONFIG_FAIR_GROUP_SCHED
	.migrate_task_rq	= migrate_task_rq_wrr, 				
#endif
*/
	.set_cpus_allowed       = set_cpus_allowed_wrr,
	.rq_online		= rq_online_wrr,
	.rq_offline		= rq_offline_wrr,
	.pre_schedule		= pre_schedule_wrr,
	.post_schedule		= post_schedule_wrr,
	.task_woken		= task_woken_wrr,

	.task_waking		= task_waking_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	.task_fork		= task_fork_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_from		= switched_from_wrr,
	.switched_to		= switched_to_wrr,

	.get_rr_interval	= get_rr_interval_wrr,
/*
#ifdef CONFIG_WRR_GROUP_SCHED
	.task_move_group	= task_move_group_wrr, 				
#endif
*/
};


