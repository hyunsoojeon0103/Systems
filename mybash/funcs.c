#include "funcs.h"
char *prompt()
{
	fprintf(stdout, "$");
	fflush(stdout);
	size_t buffSize = 0;
	size_t length = 0;
	size_t last = 0;
	char *buffer = NULL;
	/* Read from stdin until \n
	 * realloc is used instead of malloc
	 * to resize the previosuly allocated block
	 * in order to read an input of any length
	 */
	do {
		buffSize += BUFSIZ;
		buffer = realloc(buffer, buffSize);
		if (buffer == NULL)
			return NULL;
		fgets(buffer+last, buffSize, stdin);
		length = strlen(buffer);
		last = length - 1;
	} while (!feof(stdin) && buffer[last] != '\n');

	if (buffer[last] == '\n')
		buffer[last] = '\0';

	return buffer;
}
char *suggest(const struct history *ht, char *target)
{
	/* idx moves left and wraps around if it reaches the edge of the array
	 * to check history for the last 100 commands from the latest command
	 * to the oldest command in the array of size 100
	 */
	for (unsigned short i = 0; i < ht->size; ++i) {
		unsigned short idx = (ht->index+MEMORYSIZE-i-1)%MEMORYSIZE;
		char *temp = strtok(target, "/");

		while (temp != NULL) {
			if (strstr(ht->commands[idx], temp) != NULL)
				return ht->commands[idx];
			    temp = strtok(NULL, "/");
		}
	}
	return NULL;
}
int parse(char *line, struct input *in, const unsigned short segCtr)
{
	char *segment[segCtr];
	char *temp;

	/* Split the input by pipe into segments*/
	temp = strtok(line, "|");
	while (temp != NULL) {
		segment[in->segCtr] = temp;
		temp = strtok(NULL, "|");
		++in->segCtr;
	}

	assert(segCtr == in->segCtr);

	for (unsigned short i = 0; i < in->segCtr; ++i) {
		in->segments[i] = malloc(ARGSMAX * sizeof(*in->segments));
		if (in->segments[i] == NULL) {
			free(line);
			return -1;
		}
		/* Split each segment into argument pieces */
		temp = strtok(segment[i], " ");
		while (temp != NULL) {
			assert(in->argsCtr[i] <= ARGSMAX);
			unsigned short idx = in->argsCtr[i];
			unsigned short len = strlen(temp) + 1;

			in->segments[i][idx] = malloc(len * sizeof(*temp));
			strcpy(in->segments[i][in->argsCtr[i]], temp);
			++(in->argsCtr[i]);
			temp = strtok(NULL, " ");
		}
		in->segments[i][in->argsCtr[i]] = '\0';
	}
	/* input succesfully processed into struct; no longer need for line*/
	free(line);
	return 0;
}
int remember(struct history *ht, const char *command)
{
	if (ht->commands[ht->index] != NULL) /* oldest command deallocated */
		free(ht->commands[ht->index]);

	unsigned short idx = ht->index;
	unsigned short len = strlen(command) + 1;

	ht->commands[idx] = malloc(len * sizeof(*ht->commands[idx]));
	if (ht->commands[idx] == NULL)
		return -1;
	strcpy(ht->commands[idx], command);
	/* index increases until MEMORYSIZE(100) and starts again from 0 */
	ht->index = (idx + 1) % MEMORYSIZE;
	ht->size = (ht->size == MEMORYSIZE) ? ht->size : ht->size + 1;
	return 0;
}
unsigned short getSegCtr(const char *line)
{
	unsigned short res = 0;

	for (const char *it = line; *it != '\0'; ++it)
		if (*it == '|')
			++res;
	return res + 1;
}
/* Handle errors that occur in a child processe
 * and give a suggestion if the command is found in history
 */
void cErr(const struct history *ht, char *command)
{
	fprintf(stderr, "error: %s\n", strerror(errno));
	char *suggestion = suggest(ht, command);

	if (suggestion != NULL)
		fprintf(stderr, "%s: Command not found. Did you mean %s?\n",
		command, suggestion);
}
/* Handle errors that occur in the parent process
 * and deallocate memory before exit the program
 */
void pErr(const char *msg, struct history *ht, struct input *in)
{
	fprintf(stderr, "%s\n", msg);
	clear(ht, in);
	exit(EXIT_FAILURE);
}
void deallocateHist(struct history *ht)
{
	for (unsigned short i = 0; i < ht->size; ++i)
		free(ht->commands[i]);
	free(ht->commands);
}
void deallocateInput(struct input *in)
{
	for (unsigned short i = 0; i < in->segCtr; ++i) {
		for (unsigned short j = 0; j < in->argsCtr[i]; ++j)
			free(in->segments[i][j]);
		free(in->segments[i]);
	}
}
void clear(struct history *ht, struct input *in)
{
	if (ht != NULL && ht->commands != NULL)
		deallocateHist(ht);
	if (in != NULL && in->segments != NULL)
		deallocateInput(in);
}
