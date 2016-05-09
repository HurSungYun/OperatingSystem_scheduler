/*
	Implementation for weighted-round-robin policy.	
*/

#include "sched.h"
#include <linux/slab.h>


static inline struct wrr_rq *wrr_rq_of(struct sched_wrr_entity *se)
{
  return se->wrr_rq;
}

static void enqueue_wrr_entity(struct *wrr_rq)
{
	/*TODO: do something */

	list_add(

  /*TODO: do remain thing */
}


/*TODO: rearrange methods we nee */

const struct sched_class wrr_sched_class;

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flag)
{
	struct wrr_rq *wrr_rq;
	struct sched_wrr_entity *se = &p->wrr;

	wrr_rq = wrr_rq_of(se);   /* implemented above  */
	enqueue_wrr_entity(wrr_rq);  /* TODO: Are other parameters needed? */
	
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p)
{


}

static void yield_task_wrr()
{
}

static void yield_to_task_wrr()
{
}



const struct sched_class wrr_sched_class = {
	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task		= dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
	.yield_to_task		= yield_to_task_wrr,

	.check_preempt_curr	= check_preempt_wakeup,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
#ifdef CONFIG_FAIR_GROUP_SCHED
	.migrate_task_rq	= migrate_task_rq_wrr,
#endif
	.rq_online		= rq_online_wrr,
	.rq_offline		= rq_offline_wrr,

	.task_waking		= task_waking_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,
	.task_fork		= task_fork_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_from		= switched_from_wrr,
	.switched_to		= switched_to_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

#ifdef CONFIG_WRR_GROUP_SCHED
	.task_move_group	= task_move_group_wrr,
#endif
};


