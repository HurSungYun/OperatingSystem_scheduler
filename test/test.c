#include <stdio.h>
#include <linux/types.h>

#define SET_WEIGHT 384
#define GET_WEIGHT 385
#define SET_SCHEDULER 156
#define GET_SCHEDULER 157
#define SCHED_WRR 6

int main(int argc, char* argv[])
{
	pid_t id = 0;

	struct sched_param {
		int sched_priority;
	};
	struct sched_param param;
	param.sched_priority = 0;

	printf("set scheduler: %d\n", syscall(SET_SCHEDULER, id, SCHED_WRR, &param));
	printf("set scheduler ends, start set weight\n");
	printf("set weight: %d\n", syscall(SET_WEIGHT, id, 7));
	printf("get weight: %d\n", syscall(GET_WEIGHT, id));
	
	return 0;
}

