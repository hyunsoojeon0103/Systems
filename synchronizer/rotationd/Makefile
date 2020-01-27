CC := $(HOME)/./Android/Sdk/ndk/20.0.5594570/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android29-clang

INCLUDE = ./include
LDLIBS = ./lib
EXTRALIBS = -lc -lhardware

CFLAGS = -Wall -Werror -g -O2 -I$(INCLUDE)
LDFLAGS = -L$(LDLIBS) $(EXTRALIBS)

rotationd: rotationd.c rotationd.h
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $<

test: test.c rotationd.h
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $<

all: rotationd test

.PHONY: clean
clean:
	rm -f *.o test
