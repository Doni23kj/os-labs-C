CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread
all: sum_threads spawn_cost false_sharing
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
run: all
	./sum_threads 100000000 5 | tee sum_threads.csv
	./spawn_cost 10000 | tee spawn_cost.csv
	./false_sharing 200000000 | tee false_sharing.csv
clean:
	rm -f sum_threads spawn_cost false_sharing
