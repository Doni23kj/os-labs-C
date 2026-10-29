CC=gcc
CFLAGS=-O2 -Wall -Wextra
all: copy syscall_write
copy: copy.c
	$(CC) $(CFLAGS) -o $@ $<
syscall_write: syscall_write.c
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f copy syscall_write *.tmp
