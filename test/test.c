#include <stdio.h>
#include <linux/types.h>

#define SET_WEIGHT 384
#define GET_WEIGHT 385
#define SET_SCHEDULER 156
#define GET_SCHEDULER 157
#define SCHED_WRR 6
#define SCHED_NORMAL 0 

int main(int argc, char* argv[])
{
	fork();
	fork();
	fork();
	fork();

	while (1) {
	}

	return 0;
}

