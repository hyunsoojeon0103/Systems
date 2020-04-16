#ifndef ROTATION_H
#define ROTATION_H
#define MAX_WINDOW 512
#define INITIAL 0
#define EVT_TRIGGERED  1
#define DESTROYED 2
#define SIG_PENDING 3
#include <linux/types.h> //bool
#include <linux/wait.h> // wait_queue_head, define_wait, prepare_to_wait	
#include <linux/idr.h> // idr
#include <linux/uaccess.h> //copy_from_user, copy_to_user
#include <linux/kernel.h> // printk
#include <linux/slab.h> // kmalloc
#include <linux/kfifo.h> // kfifo
#include <linux/spinlock.h> //spinLock;
#include <linux/cred.h> // uid for root
struct dev_rotation {
	int x;
	int y;
	int z;
};
struct rotation_motion {
	int x;
	int y;
	int z;
	bool bidirection;
};
struct rotation_event {
	struct rotation_motion motion;
	struct wait_queue_head queue;
	unsigned short condition;
	atomic_t ctr;
	int id;
};
#endif
