#ifndef FUNCTIONS__H
#define FUNCTIONS__H

#define MEMORYSIZE 100
#define ARGSMAX 32

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

extern errno;

struct history {
	char **commands;
	unsigned short index;
	unsigned short size;
};

struct input {
	char **segments[ARGSMAX];
	unsigned short argsCtr[ARGSMAX];
	unsigned short segCtr;
};

char *prompt();
char *suggest(const struct history *ht, char *target);
int remember(struct history *ht, const char *command);
int parse(char *line, struct input *in, const unsigned short segCtr);
unsigned short getSegCtr(const char *line);
void cErr(const struct history *ht, char *command);
void pErr(const char *msg, struct history *ht, struct input *in);
void deallocateHist(struct history *ht);
void deallocateInput(struct input *in);
void clear(struct history *ht, struct input *in);
#endif
