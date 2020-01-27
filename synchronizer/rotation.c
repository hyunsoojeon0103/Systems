#include <linux/rotation.h>
#include <linux/syscalls.h> // asmlinkage

/* sample threshhold */
static int sampleSize = 6;

/* sample window size */
static int windowSize = 10;

/* 
 * create idr that helps keep track of,
 * find and remove certain events given event id
 */
static DEFINE_IDR(eventIdr);

/*
 * a queue to hold the windowSize number 
 * of the latest rotational speeds
 */
static DEFINE_KFIFO(sampleQueue,struct dev_rotation,MAX_WINDOW); 

/* create a spin lock for process synchronization */
static DEFINE_SPINLOCK(myLock);

static bool threshCheck(const struct rotation_motion *threshold)	
{
	unsigned short exceedCtr = 0;
	unsigned short i = 0;
	unsigned short qSize = kfifo_len(&sampleQueue);
	struct dev_rotation dummy;
	int xyz[3];
	while(i < qSize)
	{
		kfifo_get(&sampleQueue,&dummy);
		xyz[0] = (threshold->bidirection)? abs(dummy.x) : dummy.x;
		xyz[1] = (threshold->bidirection)? abs(dummy.y) : dummy.y;
		xyz[2] = (threshold->bidirection)? abs(dummy.z) : dummy.z;
		if(xyz[0] >= threshold->x &&
		   xyz[1] >= threshold->y &&
		   xyz[2] >= threshold->z)
			++exceedCtr;
		++i;
		kfifo_put(&sampleQueue,dummy); 
	}
	return exceedCtr >= sampleSize;
}

SYSCALL_DEFINE1(set_rotation, struct dev_rotation __user *, rotation)
{
	int id; 
	struct rotation_event *eCursor; 
	struct dev_rotation kRot; 
	struct dev_rotation dummy; 

	/* only root can update the state */
	if(current_cred()->uid.val != 0)
		return -EPERM;

	if (copy_from_user(&kRot,rotation,sizeof(struct dev_rotation)))
		return -EFAULT;
	
	/* critical section begins */
	spin_lock(&myLock);

	/* if queue is full, pop the oldest out */	
	if (kfifo_len(&sampleQueue) == windowSize)
		kfifo_get(&sampleQueue,&dummy); 
	
	/* new rotaional speed enqueued */
	kfifo_put(&sampleQueue,kRot); 
	
	/* if there are more speeds than sampleSize, check for noise */			 
	if (kfifo_len(&sampleQueue) >= sampleSize)
	{
		/* iterate all events and notify if triggered  */
		idr_for_each_entry(&eventIdr,eCursor,id)
			if(threshCheck(&eCursor->motion))
			{	
				/* change the condition and wake up processes */
				eCursor->condition = EVT_TRIGGERED;
				wake_up_all(&eCursor->queue); 
			}
	}

	/* critical section ends */
	spin_unlock(&myLock);

	return 0; 
}
SYSCALL_DEFINE1(rotevt_create, struct rotation_motion __user *, motion)
{
	struct rotation_event *event;
	long res = 0;

	event = kmalloc(sizeof(struct rotation_event),GFP_KERNEL);	
	if (event == NULL)
		return -ENOMEM;

	if (copy_from_user(&event->motion,motion,sizeof(struct rotation_motion)))
	{	
		kfree(event);
		return -EFAULT;
	}

	/* intialize the queue */
	init_waitqueue_head(&event->queue); 
	event->condition = INITIAL;
	event->ctr.counter = 0;

	/* critical section begins */
	spin_lock(&myLock);

	/* allocate an id number */
	event->id = idr_alloc(&eventIdr,event,0,-1,GFP_KERNEL);

	/* check for the result of alloc */ 
	res = event->id; 
	if(res == -ENOMEM || res == -ENOSPC)
		kfree(event);
	
	/* critical section ends */
	spin_unlock(&myLock);

	return res;
}
SYSCALL_DEFINE1(rotevt_destroy, int, event_id)
{
	struct rotation_event *targetEvt; 

	/* critical section begins */
	spin_lock(&myLock);

	targetEvt = idr_find(&eventIdr,event_id);
	if(targetEvt == NULL)
	{
		spin_unlock(&myLock);
		return -EINVAL;
	}

	targetEvt->condition = DESTROYED;

	/* if true, let the last process destroy the event */
	if(wq_has_sleeper(&targetEvt->queue))
		wake_up_all(&targetEvt->queue);
	else
	{
		targetEvt = idr_remove(&eventIdr,event_id); 
		kfree(targetEvt);
	}
	/* critical section ends */
	spin_unlock(&myLock);

	return 0;
}
SYSCALL_DEFINE1(rotevt_wait, int, event_id)
{
	DEFINE_WAIT(wait); 
	struct rotation_event *targetEvt; 
	long res = 0;
	
	/* critical section begins */
	spin_lock(&myLock);
	targetEvt = idr_find(&eventIdr,event_id);

	if(targetEvt == NULL)
	{
		spin_unlock(&myLock);
		return -EINVAL;
	}
	
	while(!targetEvt->condition)
	{
		prepare_to_wait(&targetEvt->queue,&wait,TASK_INTERRUPTIBLE);
		if (signal_pending(current))
		{
			targetEvt->condition = SIG_PENDING;	
			res = -ERESTARTSYS; 
		}
		else
		{
			atomic_add(1,&targetEvt->ctr);		

			/* critical section ends */	
			spin_unlock(&myLock);
			
			/* process going to sleep */
			schedule(); 
				
			/* critical section begins */
			spin_lock(&myLock);
		}
	}
		
	finish_wait(&targetEvt->queue,&wait); 
	
	/*
	 * whether event is triggered or destroyed 
 	 * decrement the counter. if event is to be destroyed
	 * the last process to exit will destroy the event
	 */	
	if (atomic_sub_and_test(1, &targetEvt->ctr) &&
			targetEvt->condition == DESTROYED)	
	{
		targetEvt = idr_remove(&eventIdr,event_id); 
		kfree(targetEvt);
	}

	/* critical section ends */
	spin_unlock(&myLock);

	return 0;
}
SYSCALL_DEFINE2(set_rotation_sampling, int, nr_samples, int, window)
{
	struct dev_rotation dummy;

	/* check for root */
	if(current_cred()->uid.val != 0)
		return -EPERM;
	if (nr_samples < 0 || nr_samples > window || window > MAX_WINDOW)
		return -EINVAL;
	
	/* critical section begins */	
	spin_lock(&myLock);
	
	/* discard the previous records */
	while(kfifo_len(&sampleQueue))
		kfifo_get(&sampleQueue,&dummy);
		
	sampleSize = nr_samples;
	windowSize = window;
	
	/* critical section ends */
	spin_unlock(&myLock);	

	return 0;
}
SYSCALL_DEFINE2(get_rotation_sampling, int __user *, nr_samples, int __user *,window)
{
	spin_lock(&myLock);
	if (put_user(sampleSize,nr_samples)||
		put_user(windowSize,window))
	{
		spin_unlock(&myLock);
		return -EFAULT;	
	}
	spin_unlock(&myLock);
	return 0;
}

