CC=gcc
CFLAGS=-O2 -Wall -Wextra
all: sched burn
sched: sched.c
	$(CC) $(CFLAGS) -o $@ $<
burn: burn.c
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f sched burn
