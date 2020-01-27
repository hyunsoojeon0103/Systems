#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/prinfo.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

static struct task_struct *get_root(int root_pid)
{
	if (root_pid == 0)
		return &init_task;
	return find_task_by_vpid(root_pid);
}

static long terminate(struct prinfo *pr, const long err)
{
	/* free and return error msg */
	kfree(pr);
	return err;
}

static int psCopy(struct prinfo *dst,
		  const struct task_struct *src,
		  const unsigned short lvl)
{
	/* copy information from task_struct to prinfo */
	if (dst == NULL || src == NULL)
		return -1;
	if (src->real_parent == NULL)
		dst->parent_pid = 0;
	else
		dst->parent_pid = src->real_parent->pid;
	dst->pid = src->pid;
	dst->state = src->state;
	if (src->cred == NULL)
		dst->uid = 0;
	else
		dst->uid = src->cred->uid.val;
	strcpy(dst->comm, src->comm);
	dst->level = lvl;
	return 0;
}

static unsigned short getLevel(const pid_t root, pid_t child)
{
	/* walk up tree to count levels */
	unsigned short res = 0;

	while (root != child) {
		struct task_struct *parent = get_root(child);

		child = parent->real_parent->pid;
		++res;
	}
	return res + 1;
}

static long do_ptree(struct prinfo *buf, int *nr, int root_pid)
{
	/* initialize variables */
	struct list_head *cursor = NULL;
	struct task_struct *task = NULL;
	struct task_struct *root = NULL;
	struct prinfo *tree = NULL;
	unsigned short next = 0; /* used to keep track of the next process*/
	unsigned short level = 0;
	int knr = 0; /* kernel nr */
	int i = 0;   /* index for process tree */

	/* make sure pointers are not null */
	if (buf == NULL || nr == NULL)
		return -EINVAL;

	/* copy the value to kernel space and check for success */
	if (copy_from_user(&knr, nr, sizeof(int)) != 0)
		return -EFAULT;

	/* check for valid number of entries */
	if (knr < 1)
		return -EINVAL;

	/* kmalloc memory for process tree */
	tree = kmalloc((sizeof(struct prinfo) * knr), GFP_KERNEL);
	if (tree == NULL)
		return -1;

	/* get the process given pid */
	root = get_root(root_pid);
	if (root == NULL)
		return terminate(tree, -ESRCH);

	/* critical section begins */
	read_lock(&tasklist_lock);

	/*
	 * filter out threads
	 * copy the root into prinfo
	 * if fails, release the lock and terminate
	 */
	if (root->group_leader == root) {
		if (psCopy(&tree[i], root, level) != 0) {
			read_unlock(&tasklist_lock);
			return terminate(tree, -1);
		}
		++i;
	}

	/* BFS begins */
	while (next < i && i < knr) {
		/* get the task whose child row will be traversed */
		struct task_struct *parent = get_root(tree[next].pid);

		level = getLevel(tree[0].pid, parent->pid);

		/* traverse the row of siblings */
		for (cursor = parent->children.next;
			cursor != &parent->children && i < knr;
			cursor = cursor->next) {

			/*
			 * on each iteration task points to
			 * the next sibling struct
			 */
			task = list_entry(cursor, struct task_struct, sibling);

			/*
			 * filter out threads
			 * copy the process into prinfo
			 * if fails, release the lock and terminate
			 */
			if (task->group_leader == task) {
				if (psCopy(&tree[i], task, level) != 0) {
					read_unlock(&tasklist_lock);
					return terminate(tree, -1);
				}
				++i;
			}
		}
		++next;
	}

	/* critical section ends */
	read_unlock(&tasklist_lock);

	/* copy modified nr value to user and check for success */
	if (i != knr && copy_to_user(nr, &i, sizeof(int)) != 0)
		return terminate(tree, -EFAULT);

	/* copy the value to user space and check for success */
	if (copy_to_user(buf, tree, sizeof(struct prinfo) * i) != 0)
		return terminate(tree, -EFAULT);

	kfree(tree);

	return 0;
}

SYSCALL_DEFINE3(ptree, struct prinfo *, buf, int *, nr, int, root_pid)
{
	return do_ptree(buf, nr, root_pid);
}
