#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

struct prinfo {
	pid_t parent_pid;
	pid_t pid;
	long state;
	uid_t uid;
	char comm[16];
	int level;
};

static void die(const char *msg)
{
	perror(msg);
	exit(1);
}

int main(int argc, char **argv)
{
	int nr = 0;
	int root_pid = 0;
	int bufSize = 10;
	struct prinfo *buf = NULL;

	/* read in command line arguments */
	if (argc > 2) {
		die("Usage: ./test root_pid");
	} else if (argc == 2) {

		/* check whether argv is numeric */
		for (unsigned short i = 0; i < strlen(argv[1]); i++)
			assert(argv[1][i] >= '0' && argv[1][i] <= '9');

		/* let root_pid be argument */
		root_pid = atoi(argv[1]);
	}

	/* dynamically allocate space for buf */
	while (nr < bufSize) {
		nr = bufSize;
		buf = malloc(sizeof(struct prinfo) * nr);
		if (buf == NULL)
			die("malloc failed");

		/* call ptree system call */
		if (syscall(333, buf, &nr, root_pid) != 0) {
			free(buf);
			die("syscall failed");
		}
		/* double bufSize if nr reaches bufSize*/
		if (nr == bufSize) {
			free(buf);
			bufSize *= 2;
		} else
			bufSize = nr;
	}

	/* print out contents from buffer */
	for (unsigned short i = 0; i < nr; i++)
		printf("%s,%d,%d,%d,%ld,%d\n", buf[i].comm, buf[i].pid,
				buf[i].parent_pid, buf[i].uid,
				buf[i].state, buf[i].level);

	/* free buf */
	free(buf);
	return 0;
}
