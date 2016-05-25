#include <stdio.h>
#include <linux/types.h>

#define SCHED_SETWEIGHT 384
#define SCHED_GETWEIGHT 385

int main(int argc, char* argv[])
{
	unsigned long long int i, t;
	int j, weight;
	double time, sum;
	pid_t id = 0;
	clock_t start, end;

	for (weight = 1; weight < 21; weight++) {
		printf("set weight %d: %d\t", weight, syscall(SCHED_SETWEIGHT, id, weight));
		sum = 0;
		for (j = 0; j < 5; j++) {
			t = 1874919423;
			start = clock();
			while(t != 1){
				for(i = 2; i <= t; i++){
					if(t % i == 0){
						t = t / i;
						break;
					}
				}
			}
			end = clock();
			time = (double)(end - start)/1000;
			printf("%.7f\t", time);
			sum += time;
		}
		printf("avg: %.7f\n", sum/5);
	}
	return 0;
}

