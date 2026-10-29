// Спинлок (TAS, TTAS) против pthread_mutex_t. ./spin [ITERS_SHORT] [ITERS_LONG]
// Короткая критическая секция: один инкремент. Длинная: ~LONG_WORK пустых итераций внутри секции.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(__i386__)
#define RELAX() __builtin_ia32_pause()
#elif defined(__aarch64__)
#define RELAX() __asm__ __volatile__("yield")
#else
#define RELAX() ((void)0)
#endif

typedef struct { volatile int f; } spin_t;
static void tas_lock(spin_t *l)  { while (__atomic_exchange_n(&l->f, 1, __ATOMIC_ACQUIRE)) RELAX(); }
static void ttas_lock(spin_t *l) {
    for (;;) {
        while (__atomic_load_n(&l->f, __ATOMIC_RELAXED)) RELAX();     // крутимся на чтении (кэш), не на записи
        if (!__atomic_exchange_n(&l->f, 1, __ATOMIC_ACQUIRE)) return;
    }
}
static void spin_unlock(spin_t *l) { __atomic_store_n(&l->f, 0, __ATOMIC_RELEASE); }

enum { TAS, TTAS, MUTEX };
static const char *names[] = {"tas", "ttas", "mutex"};
static spin_t sl; static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static long long counter; static long iters, work; static int kind;
static pthread_barrier_t bar;

static void *th(void *a) {
    (void)a;
    pthread_barrier_wait(&bar);
    for (long i = 0; i < iters; i++) {
        if (kind == TAS) tas_lock(&sl); else if (kind == TTAS) ttas_lock(&sl); else pthread_mutex_lock(&mx);
        counter++;
        for (volatile long w = 0; w < work; w++) ;
        if (kind == MUTEX) pthread_mutex_unlock(&mx); else spin_unlock(&sl);
    }
    return NULL;
}
static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }

int main(int argc, char **argv) {
    long is = argc > 1 ? atol(argv[1]) : 1000000, il = argc > 2 ? atol(argv[2]) : 20000;
    int Ts[] = {2, 4, 16};
    printf("# nproc=%ld short_iters=%ld long_iters=%ld\n", sysconf(_SC_NPROCESSORS_ONLN), is, il);
    printf("section,threads,lock,seconds,correct\n");
    for (int s = 0; s < 2; s++) for (int ti = 0; ti < 3; ti++) for (kind = TAS; kind <= MUTEX; kind++) {
        int T = Ts[ti]; iters = s ? il : is; work = s ? 1000 : 0; counter = 0;
        pthread_t t[16]; pthread_barrier_init(&bar, NULL, T + 1);
        for (int i = 0; i < T; i++) pthread_create(&t[i], NULL, th, NULL);
        pthread_barrier_wait(&bar); double t0 = now();
        for (int i = 0; i < T; i++) pthread_join(t[i], NULL);
        double dt = now() - t0; pthread_barrier_destroy(&bar);
        printf("%s,%d,%s,%.3f,%s\n", s ? "long" : "short", T, names[kind], dt, counter == (long long)T * iters ? "yes" : "NO");
        fflush(stdout);
    }
    return 0;
}
