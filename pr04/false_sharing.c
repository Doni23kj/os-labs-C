// Ложное разделение кэш-линий: 2 потока увеличивают СВОИ счётчики.
// Вариант A: счётчики рядом (одна 64-байтная линия). Вариант B: разнесены на 64 байта.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static long long ITER = 200000000LL;
typedef struct { volatile long long *p; } Arg;
static void *work(void *a) { volatile long long *p = ((Arg *)a)->p; for (long long i = 0; i < ITER; i++) (*p)++; return NULL; }

static double run(volatile long long *c0, volatile long long *c1) {
    pthread_t t[2]; Arg a[2] = {{c0}, {c1}};
    double t0 = now();
    pthread_create(&t[0], NULL, work, &a[0]); pthread_create(&t[1], NULL, work, &a[1]);
    pthread_join(t[0], NULL); pthread_join(t[1], NULL);
    return now() - t0;
}
int main(int argc, char **argv) {
    if (argc > 1) ITER = atoll(argv[1]);
    int reps = 3;
    long long *buf = aligned_alloc(64, 256);
    for (int i = 0; i < 32; i++) buf[i] = 0;
    printf("# iter=%lld nproc=%ld\n", ITER, sysconf(_SC_NPROCESSORS_ONLN));
    printf("layout,rep,seconds\n");
    for (int r = 0; r < reps; r++) printf("adjacent(same line),%d,%.3f\n", r, run(&buf[0], &buf[1]));
    for (int r = 0; r < reps; r++) printf("padded(64B apart),%d,%.3f\n", r, run(&buf[0], &buf[8]));
    free(buf);
    return 0;
}
