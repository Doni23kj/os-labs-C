CC=gcc
CFLAGS=-O2 -g -Wall -Wextra -pthread
philosophers: philosophers.c
	$(CC) $(CFLAGS) -o $@ $<
all-variants: philosophers
	for v in naive ordered trylock-fixed trylock-rand waiter; do ./philosophers $$v 10 | tee stats_$$v.txt; echo; done
tsan:
	$(CC) -O1 -g -Wall -Wextra -pthread -fsanitize=thread -o philosophers_tsan philosophers.c
	for v in naive ordered waiter; do ./philosophers_tsan $$v 3; done 2>&1 | tee tsan_report.txt
helgrind: philosophers
	valgrind --tool=helgrind ./philosophers naive 5 2> helgrind_naive.txt; tail -30 helgrind_naive.txt
clean:
	rm -f philosophers philosophers_tsan stats_*.txt tsan_report.txt helgrind_naive.txt
