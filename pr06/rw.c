// Читатели–писатели на pthreads. ./rw reader|fair [секунды=3]
//  reader — приоритет читателей (писатель может голодать)
//  fair   — писатель, ждущий очереди, блокирует новых читателей (нет голодания писателя)
// Инвариант: писатель делает a++ ... b=a с паузой между; читатель всегда обязан видеть a==b.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    pthread_mutex_t m; pthread_cond_t cr, cw;
    int readers, writing, waiting_w, prefer_readers;
} rw_t;
static rw_t L = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, 0, 0 };

static void rd_lock(rw_t *l) {
    pthread_mutex_lock(&l->m);
    if (l->prefer_readers) while (l->writing) pthread_cond_wait(&l->cr, &l->m);
    else while (l->writing || l->waiting_w > 0) pthread_cond_wait(&l->cr, &l->m);
    l->readers++;
    pthread_mutex_unlock(&l->m);
}
static void rd_unlock(rw_t *l) {
    pthread_mutex_lock(&l->m);
    if (--l->readers == 0) pthread_cond_signal(&l->cw);
    pthread_mutex_unlock(&l->m);
}
static void wr_lock(rw_t *l) {
    pthread_mutex_lock(&l->m);
    l->waiting_w++;
    while (l->writing || l->readers > 0) pthread_cond_wait(&l->cw, &l->m);
    l->waiting_w--; l->writing = 1;
    pthread_mutex_unlock(&l->m);
}
static void wr_unlock(rw_t *l) {
    pthread_mutex_lock(&l->m);
    l->writing = 0;
    if (l->prefer_readers) { pthread_cond_broadcast(&l->cr); pthread_cond_signal(&l->cw); }
    else if (l->waiting_w > 0) pthread_cond_signal(&l->cw);
    else pthread_cond_broadcast(&l->cr);
    pthread_mutex_unlock(&l->m);
}

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static int stop;                                    // доступ только через __atomic (находка TSan: volatile — не синхронизация)
static long a, b;                                   // защищены L
static long reads[8], violations, writes; static double max_wait;

static void *reader(void *p) {
    long id = (long)p;
    while (!__atomic_load_n(&stop, __ATOMIC_RELAXED)) {
        rd_lock(&L);
        if (a != b) __atomic_add_fetch(&violations, 1, __ATOMIC_RELAXED);   // инвариант
        reads[id]++;
        usleep(500);                                // держим блокировку, чтобы читатели перекрывались
        rd_unlock(&L);
    }
    return NULL;
}
static void *writer(void *p) {
    (void)p;
    while (!__atomic_load_n(&stop, __ATOMIC_RELAXED)) {
        double t0 = now();
        wr_lock(&L);
        double w = now() - t0; if (w > max_wait) max_wait = w;
        a++; usleep(100); b = a;
        writes++;
        wr_unlock(&L);
        usleep(2000);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2 || (strcmp(argv[1], "reader") && strcmp(argv[1], "fair"))) { fprintf(stderr, "usage: %s reader|fair [sec]\n", argv[0]); return 1; }
    L.prefer_readers = !strcmp(argv[1], "reader");
    int sec = argc > 2 ? atoi(argv[2]) : 3, R = 4;
    pthread_t r[8], w;
    for (long i = 0; i < R; i++) pthread_create(&r[i], NULL, reader, (void *)i);
    pthread_create(&w, NULL, writer, NULL);
    sleep(sec); __atomic_store_n(&stop, 1, __ATOMIC_RELAXED);
    pthread_join(w, NULL); for (int i = 0; i < R; i++) pthread_join(r[i], NULL);
    long tr = 0; for (int i = 0; i < R; i++) tr += reads[i];
    printf("mode=%s secs=%d readers=%d  reads=%ld  writes=%ld  max_writer_wait=%.3fs  invariant_violations=%ld\n",
           argv[1], sec, R, tr, writes, max_wait, violations);
    return 0;
}
