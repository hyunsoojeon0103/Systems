#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>
#include "rotationd.h"

#define SET_ROTATION 333
#define ROTEVT_CREATE 334
#define ROTEVT_DESTROY 335
#define ROTEVT_WAIT 336
#define SET_ROTATION_SAMPLING 337
#define GET_ROTATION_SAMPLING 338
static void terminate(const int err)
{
	fprintf(stderr,"%s\n",strerror(err));
	exit(1);
}
int main(void)
{
	/* a random generator seed */
	srand((unsigned)time(NULL));

	const unsigned short NUM_OF_PROCS = 100;
	const unsigned short MAXSPEED = 50;
	const unsigned short NUM_OF_EVTS = 20; 
	const unsigned short HINDER_ATTEMPS = 30;
		
	struct rotation_motion threshold[NUM_OF_EVTS];
	long event[NUM_OF_EVTS];
	syscall(SET_ROTATION_SAMPLING,10,20);
	/* create events */
	for(unsigned short i = 0; i < NUM_OF_EVTS; ++i)
	{
		/* assign values for thresholds */
		threshold[i].x = (rand() % 2 == 0)? rand() % MAXSPEED: (rand() % MAXSPEED) * -1;
		threshold[i].y = (rand() % 2 == 0)? rand() % MAXSPEED: (rand() % MAXSPEED) * -1;
		threshold[i].z = (rand() % 2 == 0)? rand() % MAXSPEED: (rand() % MAXSPEED) * -1;

		/* 90 percent chance of bidirection */
		threshold[i].bidirection = rand() % 10 > 0;

		/* create an event with the threshold and store the event id */
		event[i] = syscall(ROTEVT_CREATE,&threshold[i]);
		fprintf(stdout,"event.%ld has been created with threshold { x = %d, y = %d, z = %d }\n",event[i],threshold[i].x,threshold[i].y,threshold[i].z);
	} 
		
	pid_t pid;

	/* create processes and wait on events */
	for(unsigned short i = 0; i < NUM_OF_PROCS; ++i)
	{
		pid = fork();
		if(pid < 0)
			fprintf(stderr,"fork failed\n");
		else if(pid == 0)
		{
			/* different seeds for each process */  
			srand((unsigned)time(NULL) ^ (getpid() << 16));

			/* process waits on an event */
			unsigned short evtIdx = rand() % NUM_OF_EVTS;
			fprintf(stdout,"process.%hu waits on event.%ld\n",i,event[evtIdx]);
			long res = syscall(ROTEVT_WAIT,event[evtIdx]);
			if (res != 0)
				terminate(res);
			else	/* processes have woken up */
			{
				fprintf(stdout,"%hu: x = %d, y = %d, z = %d\n",
			 	i,threshold[evtIdx].x, threshold[evtIdx].y, threshold[evtIdx].z);
			}
			return 0;
		}
	}


	/* create another process to mess with syscalls while children are waiting on events */	
	pid = fork();
	if (pid < 0)
		fprintf(stderr,"second fork failed\n");
	else if(pid == 0)
	{
		/* child to process that will call syscalls randomly */
		sleep(1);

		/* detach from root for permission test */
		setuid(9999);
		char *testCase[4] = {"set_rotation test",
				     "rotevt_destroy test",
				     "set_rotation_sampling test",
				     "get_rotation_sampling test"};
		for(unsigned short i =0; i < HINDER_ATTEMPS; ++i)
		{
			/* extensive test on set_rotation, evt_destroy and set/get sampling */
			long res;
			int tmpId;
			int sample;
			int window;
			struct dev_rotation tmp = {.x=rand(),.y=rand(),.z=rand()};

			unsigned short idx = rand() % 4;
			switch(idx)
			{
				case 0:
					/* expectation : the request should be rejected since not root */
					res = syscall(SET_ROTATION,&tmp);
					if(res != 0)
						fprintf(stdout,"%-40s[PASSED]:%s\n",testCase[idx],strerror(res));
					else
						fprintf(stdout,"%-40s[FAILED]\n",testCase[idx]);
					break;
				case 1:
					/* expectation : either event cannot be found or an event be destroyed */ 
					tmpId = rand() % (NUM_OF_EVTS * 1); 
					res = syscall(ROTEVT_DESTROY,tmpId);
					if (res != 0)
						fprintf(stdout,"%-40s[PASSED]:%s\n",testCase[idx],strerror(res));
					else
						fprintf(stdout,"event.%d has been destroyed as a hinderance\n",tmpId);
					break;
				case 2:
					/* expectation : the request should be rejected since not root */
					/* 20 percent chance of negative */
					/* negative values should be rejected */
					sample = (rand() % 5 == 0)? rand() * -1: rand(); 
					window = (rand() % 5 == 0)? rand() * -1: rand();
					res = syscall(SET_ROTATION_SAMPLING,sample,window);
					if (res != 0)
						fprintf(stdout,"%-40s[PASSED]:%s\n",testCase[idx],strerror(res));
					else
						fprintf(stdout,"%-40s[FAILED]\n",testCase[idx]);
					break;
				case 3:
					/* expectation : sample and window will be updated */ 
					res = syscall(GET_ROTATION_SAMPLING,&sample,&window);
					if(res == 0)
						fprintf(stdout,"%-40s[PASSED]:sample = %d, window = %d\n",testCase[idx],sample,window);
					else
						fprintf(stdout,"%-40s[FAILED] %s\n",testCase[idx],strerror(res));
					break;	
			}
		}
		return 0;	
	}
	else
	{	/* destroy the events */
		sleep(60);
		for(unsigned short i = 0; i < NUM_OF_EVTS; ++i)
		{
			if(syscall(ROTEVT_DESTROY,event[i]) != 0)
				fprintf(stderr,"event %ld cannot be found\n",event[i]);
			else
				fprintf(stdout,"event %ld has been destroyed\n",event[i]);
		}
	}

	sleep(1);

	return 0;
}
