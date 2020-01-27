#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#define MAX_CPUS 8
static void terminate(const char *msg)
{
	perror(msg);
	exit(1);
}
struct wrr_info{
	int num_cpus;
	int nr_running[MAX_CPUS];
	int total_weight[MAX_CPUS];
};
static void runInf(const unsigned short NUMOFPROCS);
static void display(const struct wrr_info *info);
static void setWeight(const int weight);
int main(int argc, char **argv)
{
	assert(argc >= 2);
	if(strcmp(argv[1],"-g") == 0)
	{
		struct wrr_info info; 	
		if(syscall(333,&info) == 0)
			display(&info);
		else
			terminate("getting wrr info failed");
	}
	else if(strcmp(argv[1],"-sw") == 0)
	{
		assert(argc == 3);
		if(syscall(334,atoi(argv[2])) == 0)
			fprintf(stdout,"setting weight to %s\n",argv[2]);
		else
			terminate("setting weight failed");
	}
	else if(strcmp(argv[1],"-r") == 0)
	{
		const unsigned short NUMOFPROCS = argc == 2? 1: atoi(argv[2]);
		runInf(NUMOFPROCS);
	}
	else
		fprintf(stderr,"invalid usage\n");

	return 0;
}
static void setWeight(const int weight)
{
	if(syscall(334,weight))
		terminate("setting weight failed");
}
static void display(const struct wrr_info *info)
{
	for (unsigned short i = 0; i < info->num_cpus; ++i)
		fprintf(stdout,"CPU[%d]\nnr_running: %d\ntotal_weight:%d\n",
			i,info->nr_running[i],info->total_weight[i]);
	printf("\n");
}
static void runInf(const unsigned short NUMOFPROCS)
{
	struct sched_param param = {.sched_priority = 0};
	unsigned short ctr = 0;
	FILE *fp;
	int pshared;
	pid_t pid;
	pid_t childPid;
	pthread_spinlock_t lock;
	
	if (pthread_spin_init(&lock,pshared))
		terminate("spinlock error");
	
	fp = fopen("pid.txt","a");
	for(unsigned short i=0; i < NUMOFPROCS; ++i)
	{
		pid = fork();
		if(pid < 0)
			terminate("Fork failed");
		else if(pid == 0)
		{
			childPid = getpid();
			srand((unsigned)time(NULL) ^ (childPid << 16));
			//setWeight(rand() % 40 + 10);
			setWeight(13);
			pthread_spin_lock(&lock);
			fprintf(fp,"%d\n",childPid);
			fclose(fp);
			++ctr;
			pthread_spin_unlock(&lock);
			if (ctr == NUMOFPROCS)
				pthread_spin_destroy(&lock);
			if(syscall(144,childPid,7,&param))
				terminate("syscall failed");
			fprintf(stdout,"Process with pid[%d] goes to an infinite loop\n",childPid);

			if(i% 3 == 0)
				while(1);
			else
			{
				int j = 0xFFFFFFFF;
				while(--j);
				fprintf(stderr,"EXITING~~~\n");
				exit(0);
			}
		}
	}
	fclose(fp);
}
