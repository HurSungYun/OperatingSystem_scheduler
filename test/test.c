#include <stdio.h>
#include <linux/types.h>

#define SCHED_SETWEIGHT 384
#define SCHED_GETWEIGHT 385

int main(int argc, char* argv[])
{
	pid_t id = 0;
	printf("%d\n", syscall(SCHED_SETWEIGHT, id, 100));
	printf("%d\n", syscall(SCHED_GETWEIGHT, id));
	return 0;
}

