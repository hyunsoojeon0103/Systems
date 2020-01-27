CC = gcc

CFLAGS = -c -g -Wall

LFLAGS = -g -o

myBash: shell.o funcs.o
	$(CC) $(LFLAGS) myBash shell.o funcs.o

shell.o: shell.c funcs.c
	$(CC) $(CFLAGS) shell.c

funcs.o: funcs.c
	$(CC) $(CFLAGS) funcs.c

.PHONY: clean
clean:
	rm -f *.o myBash 
