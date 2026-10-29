CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread
ipc_bench: ipc_bench.c
	$(CC) $(CFLAGS) -o $@ $< -lrt
bench: ipc_bench
	./bench.sh
check:
	@ls /dev/shm /dev/mqueue 2>/dev/null | grep ipcbench || echo "чисто: ресурсов IPC не осталось"
clean:
	rm -f ipc_bench
