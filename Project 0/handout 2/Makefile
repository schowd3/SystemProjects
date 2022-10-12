all: scheduler

CFLAGS=-g -Og -Wall -Werror -Wno-error=unused-variable -std=gnu99
CC=gcc

scheduler: context.c scheduler.c sys.c clock.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f scheduler
