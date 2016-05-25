project 3: Weighted Round-Robin scheduler for linux kernel
==========================================================
# High-level design and Implementation

## Data Structures and Functions used in sched\_class
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

## Load balancing algorithm

* Load balancing between rqs are done in scheduler\_tick in core.c.

* First, there exists a global variable to store the last balancing time. At each tick, cpus try to get the balance\_lock and check if it is time for load balancing. If it is, the cpu that checked the timestamp updates the timestamp, releases the balance\_lock and starts load balancing.

* Load balancing starts with finding the MAX and MIN RQ. Afterwards, the locks for the two runqueues are acquired, and the function tries to find a migratable task with maximum weight. 

* If there exists a migratable task, the function migrates the task to MIN RQ and unlocks those two locks.

## Investigation

We did the investigation with two test programs. One, test.c forks itself into 16 tasks and loops infinitely. The other one, trial.c does the trial division for 1874919423 which can be divided into 3, 13 and 48074857. The last number 4804867 makes the program run for a long time. By this test, we found several relations between program weight and execution time.

* The execution time decreases when program weight increase. In our result, the relation between weight and execution time was t = 194393\*x^(-0.923) with r^2 value 0.982. 

* We inferred the reason of the result. When the weight increases, the time slice for the process increases and the cpu runs our code for a longer time before it switches to other tasks. So the number of interrupts during the process is smaller and the waiting time is shorter.

* With the result, we can see that our scheduler works well.
	
# Lessons learned.

* SUNG-YUN HUR: It is really difficult to add new scheduler without guidance. Also, I realized I still have a lot of things to learn.
* EUN-HYANG KIM: Learned the structure of scheduler.
* YEON-WOO KIM: Fixing the kernel code is difficult and often dangerous; Always look and look again for possible bugs before testing.

