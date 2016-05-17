#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>

/* Set the SCHED_WRR weight of process, as identified by 'pid'.
 * If 'pid' is 0, set the weight for the calling process.
 * System call number 384.
 */

DEFINE_SPINLOCK(weight_lock);

int sched_setweight(pid_t pid, int weight)
{
	struct task_struct *p;
	kuid_t rootUid;
	rootUid.val = 0;

	if (weight < 0) 
		return -EINVAL;
	if (!uid_eq(current->cred->euid, rootUid) && current->pid != pid)
		return -EINVAL;

	if (pid == 0) {
		/* set calling process weight */
		p = current;
		spin_lock(&weight_lock);
		p->wrr.weight = weight;
		spin_unlock(&weight_lock);

		return 0;
	} else {
		p = pid_task(find_vpid(a->pid), PIDTYPE_PID);
		if (p == NULL)
			return -EINVAL;
		if (p->policy != SCHED_WRR) 
			return -EINVAL;
		
		spin_lock(&weight_lock);
		p->wrr.weight = weight;
		spin_unlock(&weight_lock);
	}
	return 0;
}

/* Obtain the SCHED_WRR weight of a process as identified by 'pid'.
 * If 'pid' is 0, return the weight of the calling process.
 * System call number 385.
 */
int sched_getweight(pid_t pid) {
	struct task_struct *p;
	int weight;

	if (pid == 0) {
		/* set calling process weight */
		spin_lock(&weight_lock);
		weight = current->wrr.weight;
		spin_unlock(&weight_lock);

		return weight;
	} 
	else {
		p = pid_task(find_vpid(a->pid), PIDTYPE_PID);
		if (p == NULL)
			return -EINVAL;
		if (p->policy != SCHED_WRR) 
			return -EINVAL;
		
		spin_lock(&weight_lock);
		weight = p->wrr.weight;
		spin_unlock(&weight_lock);
		
		return weight;
	}
}
