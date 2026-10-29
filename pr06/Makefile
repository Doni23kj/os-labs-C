CC=gcc
CFLAGS=-O1 -g -Wall -Wextra -pthread
PROGS=spin rw bounded
all: $(PROGS)
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
# ThreadSanitizer (нужен libtsan: sudo apt install gcc; обычно уже есть)
tsan: rw.c bounded.c
	$(CC) -O1 -g -Wall -Wextra -pthread -fsanitize=thread -o rw_tsan rw.c
	$(CC) -O1 -g -Wall -Wextra -pthread -fsanitize=thread -o bounded_tsan bounded.c
	./bounded_tsan monitor && ./bounded_tsan sem && ./rw_tsan fair 2 && ./rw_tsan reader 2
helgrind: all
	valgrind --tool=helgrind -q ./bounded monitor
	valgrind --tool=helgrind -q ./bounded sem
	valgrind --tool=helgrind -q ./rw fair 2
clean:
	rm -f $(PROGS) rw_tsan bounded_tsan spin_tsan
