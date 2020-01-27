#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "funcs.h"
int main(void)
{
	struct history ht;

	ht.index = 0;
	ht.size = 0;
	ht.commands = malloc(MEMORYSIZE * sizeof(*ht.commands));
	if (ht.commands == NULL)
		pErr("malloc failed", &ht, NULL);

	memset(ht.commands, '\0', MEMORYSIZE * sizeof(*ht.commands));

	while (1) {
		/* Display $ and read input from stdin */
		char *line = prompt();

		if (line == NULL)
			pErr("getline failed", &ht, NULL);

		/* Step through the input and check how many
		 * pipes are present, which will determine
		 * the number of programs to be run
		 */
		unsigned short segCtr = getSegCtr(line);

		struct input in;
		int status = 0;
		pid_t pid;

		memset(in.segments, '\0', segCtr*sizeof(*in.segments));
		memset(in.argsCtr, 0, segCtr*sizeof(*in.argsCtr));

		in.segCtr = 0;

		/* Parse through the input, split it by pipe
		 * and store them in an array of char **
		 */
		if (parse(line, &in, segCtr) == -1)
			pErr("malloc failed", &ht, &in);

		/* Success keeps track of the excution result of
		 * the sequence of child processes. If one child in
		 * the sequence fails, success is 0, else 1
		 */
		unsigned short success = 1;
		int pipefd[2];
		int oldFdIn = 0;

		for (unsigned short i = 0; i < in.segCtr; ++i) {
			if (strcmp(in.segments[i][0], "exit") == 0) {
				clear(&ht, &in);
				exit(EXIT_SUCCESS);
			} else if (strcmp(in.segments[i][0], "cd") == 0) {
				if (in.argsCtr[i] == 2)
					chdir(in.segments[i][1]);
				else {
					fprintf(stderr, "error: too %s args\n",
					(in.argsCtr[i] < 2) ? "few" : "many");
					success = 0;
				}
			} else {
				pipe(pipefd);
				pid = fork();
				if (pid < 0) {
					fprintf(stderr, "error: fork failed\n");
					close(pipefd[0]);
					close(pipefd[1]);
					success = 0;
				} else if (pid == 0) {
					dup2(oldFdIn, 0);
					/* if not the last arg
					 * redirect stdout
					 * for process communication
					 */
					if (i + 1 != in.segCtr)
						dup2(pipefd[1], 1);
					close(pipefd[0]);

					char **command = in.segments[i];

					execv(command[0], command);
					cErr(&ht, in.segments[i][0]);
					clear(&ht, &in);
					exit(EXIT_FAILURE);
				} else {
					wait(&status);
					close(pipefd[1]);
					/* keep fdIn for the next process */
					oldFdIn = pipefd[0];
					success = (success && !status) ? 1 : 0;
				}
			}
		}
		/* Add the commands to history
		 * only if the result of the sequence was successful
		 */
		for (unsigned short i = 0; i < in.segCtr && success; ++i)
			if (remember(&ht, in.segments[i][0]) == -1)
				pErr("malloc failed", &ht, &in);
		clear(NULL, &in);
	}
}
