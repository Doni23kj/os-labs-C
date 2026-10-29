// Параллельная сумма массива на N pthreads. ./sum_threads [ELEMENTS] [REPEAT]
// Для каждого N = 1,2,4,8,16 печатает медиану времени (только сама сумма, без выделения/заполнения).
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef struct { const int32_t *a; size_t lo, hi; long long sum; } Task;
// результат пишем один раз в конце, поэтому ложного разделения кэш-линий внутри цикла нет;
// каждая структура всё равно выровнена на 64 байта
typedef struct { Task t; } __attribute__((aligned(64))) PaddedTask;

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }

static void *worker(void *p) {
    Task *t = p; long long s = 0;
    for (size_t i = t->lo; i < t->hi; i++) s += t->a[i];
    t->sum = s;
    return NULL;
}
static int cmp(const void *x, const void *y) { double a = *(double *)x, b = *(double *)y; return (a > b) - (a < b); }

int main(int argc, char **argv) {
    size_t n = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000000ULL;
    int rep = argc > 2 ? atoi(argv[2]) : 5;
    int32_t *a = malloc(n * sizeof *a);
    if (!a) { perror("malloc"); return 1; }
    long long expect = 0;
    for (size_t i = 0; i < n; i++) { a[i] = (int32_t)(i % 100); expect += a[i]; }
    printf("# elements=%zu repeat=%d nproc=%ld\n", n, rep, sysconf(_SC_NPROCESSORS_ONLN));
    printf("threads,median_s,speedup,correct\n");
    double base = 0;
    int counts[] = {1, 2, 4, 8, 16};
    for (int ci = 0; ci < 5; ci++) {
        int N = counts[ci];
        double *times = malloc(rep * sizeof *times); int ok = 1;
        for (int r = 0; r < rep; r++) {
            PaddedTask *tk = calloc(N, sizeof *tk); pthread_t *th = malloc(N * sizeof *th);
            double t0 = now();
            for (int i = 0; i < N; i++) {
                tk[i].t.a = a; tk[i].t.lo = n * i / N; tk[i].t.hi = n * (i + 1) / N;
                pthread_create(&th[i], NULL, worker, &tk[i].t);
            }
            long long total = 0;
            for (int i = 0; i < N; i++) { pthread_join(th[i], NULL); total += tk[i].t.sum; }
            times[r] = now() - t0;
            if (total != expect) ok = 0;
            free(tk); free(th);
        }
        qsort(times, rep, sizeof *times, cmp);
        double med = times[rep / 2];
        if (N == 1) base = med;
        printf("%d,%.4f,%.2f,%s\n", N, med, base / med, ok ? "yes" : "NO");
        free(times);
    }
    free(a);
    return 0;
}
