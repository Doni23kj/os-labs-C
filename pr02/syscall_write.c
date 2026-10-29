// write() напрямую через syscall(SYS_write, ...) без libc-обёртки
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    const char *a = "A: write() из libc\n", *b = "B: syscall(SYS_write)\n";
    long r1 = write(1, a, strlen(a));
    long r2 = syscall(SYS_write, 1, b, strlen(b));
    fprintf(stderr, "write -> %ld байт, syscall(SYS_write) -> %ld байт\n", r1, r2);
    return 0;
}
