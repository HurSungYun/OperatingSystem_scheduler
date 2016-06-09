#include <stdio.h>
#include <linux/types.h>

#define SCHED_SETWEIGHT 384
#define SCHED_GETWEIGHT 385

int main(int argc, char* argv[])
{
	int i, t, weight;
	pid_t id = 0;
	clock_t start, end;

	if (argc != 3) {
		printf("Need two arguments");
		return -1;
	}

	t = atoi(argv[1]);
	weight = atoi(argv[2]);

	printf("set weight: %d\n", syscall(SCHED_SETWEIGHT, id, weight));

	start = clock();
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
	end = clock();
	printf("execution time: %.7f\n", (double)(end-start)/1000);
	return 0;
}

