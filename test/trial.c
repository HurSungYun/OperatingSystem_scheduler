#include <stdio.h>
#include <linux/types.h>

#define SCHED_SETWEIGHT 384
#define SCHED_GETWEIGHT 385

int main(int argc, char* argv[])
{
	int i, flag, t;
	pid_t id = 0;

	if (argc != 2) {
		printf("Need an argument");
		return -1;
	}

	t = atoi(argv[1]);

	printf("set weight: %d\n", syscall(SCHED_SETWEIGHT, id, 100));

	if (t == 1) printf("1\n");
	while(t != 1){
		for(i = 2; i <= t; i++){
			if(t % i == 0){
				printf("%d ",i);
				t = t / i;
				break;
			}
		}
	}
	printf("\n");
	return 0;
}

