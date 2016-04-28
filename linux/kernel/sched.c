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
int sched_setweight(pid_t pid, int weight)
{
  return 384;
}

/* Obtain the SCHED_WRR weight of a process as identified by 'pid'.
 * If 'pid' is 0, return the weight of the calling process.
 * System call number 385.
 */
int sched_getweight(pid_t pid){
  return 385;
}
