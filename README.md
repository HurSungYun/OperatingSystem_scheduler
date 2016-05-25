project 3: Weighted Round-Robin scheduler for linux kernel
==========================================================
# High-level design and Implementation

* Two data structures are newly defined to support the newly defined policy in the kernel: wrr\_rq and sched\_wrr\_entity.

* wrr\_rq is defined in kernel/sched/sched.h and consists of a run queue of sched\_wrr\_entity, a cursor to point to the current head of the circular list, and the sum of weight of the entities in the run queue. wrr\_rq structure is included in the existing struct rq to support wrr policy.

* sched\_wrr\_entity is defined in include/linux/sched.h and consists of its weight, time slice, and a list head to be included in wrr\_rq. sched\_wrr\_entity structure is included in the existing struct task\_struct to support the policy.

* Among the various methods defined sched\_class, our wrr\_sched\_class uses the following: select\_task\_rq; enqueue\_task; dequeue\_task; pick\_next\_task; task\_tick; task\_fork; switched\_to; get\_rr\_interval

* enqueue\_task, dequeue\_task, pick\_next\_task are all implemented according to the policy of weighted round-robin.

* In enqueue\_task, if no task is currently in the run queue, add the task to the run queue and point the cursor to the task. Else, add the task right before the cursor.

* In dequeue\_task, if the task that needs to be deleted is pointed by the cursor, update the cursor to the task right next to the current cursor. Else, simply delete the task.

* In pick\_next\_task, simply return the task current cursor is pointing to.

* In task\_tick, the time slice of the currently running task is decreased every time. If the time slice is zero, move the cursor to the next task and reschedule.

* task\_fork resets the timeslice of the forked task. switched\_to sets weight and timeslice to default value, and get\_rr\_interval returns the weight of the task multiplied by WRR\_TIMESLICE.

	
# Lessons learned.

* SUNG-YUN HUR:
* EUN-HYANG KIM:
* YEON-WOO KIM:

