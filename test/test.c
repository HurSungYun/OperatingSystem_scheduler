#include <stdio.h>
#include <linux/types.h>
#include <linux/sched.h>

#define SET_WEIGHT 384
#define GET_WEIGHT 385
#define SET_SCHEDULER 156
#define GET_SCHEDULER 157

int main(int argc, char* argv[])
{
	pid_t id = 0;
	struct sched_param param;

	printf("set scheduler: %d\n", syscall(SET_SCHEDULER, id, SCHED_WRR, &param));
	printf("set weight: %d\n", syscall(SET_WEIGHT, id, 100));
	
	return 0;
}

