// Обедающие философы на pthreads (5 философов, 5 вилок = мьютексы).
//   ./philosophers VARIANT [секунды=10]
// VARIANT: naive | ordered | trylock-fixed | trylock-rand | waiter
//   naive          — все берут сначала левую вилку, потом правую  -> DEADLOCK
//   ordered        — вилки берутся в порядке номеров (меньший первым) -> цикла ожидания нет
//   trylock-fixed  — левая lock, правая trylock; при неудаче вернуть левую и ждать ОДИНАКОВОЕ время -> возможен LIVELOCK
//   trylock-rand   — то же, но задержка случайная -> livelock вероятностно исчезает
//   waiter         — «официант»: семафор N-1, за столом не более 4 философов -> deadlock невозможен
// PHIL_NODETECT=1 — не определять отсутствие прогресса (программа «как есть» зависнет; для демонстрации deadlock).
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define N 5
typedef enum { NAIVE, ORDERED, TRY_FIXED, TRY_RAND, WAITER } Variant;
static Variant var;
static pthread_mutex_t fork_m[N];
static sem_t waiter;
static pthread_barrier_t start_bar;
static int stop_flag;

typedef struct { long meals; long long max_wait_ns; long long hungry_since; unsigned seed; } Phil;
static Phil ph[N];

static long long now_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000000000LL + ts.tv_nsec; }
static int stopped(void) { return __atomic_load_n(&stop_flag, __ATOMIC_RELAXED); }

static void take_forks(int i) {
    int l = i, r = (i + 1) % N;
    switch (var) {
    case NAIVE:
        pthread_mutex_lock(&fork_m[l]);
        usleep(200);                                   // «задумался» с левой вилкой в руке — делает deadlock почти неизбежным
        pthread_mutex_lock(&fork_m[r]);
        break;
    case ORDERED: {
        int a = l < r ? l : r, b = l < r ? r : l;
        pthread_mutex_lock(&fork_m[a]); pthread_mutex_lock(&fork_m[b]);
        break; }
    case TRY_FIXED: case TRY_RAND:
        for (;;) {
            if (stopped()) return;
            pthread_mutex_lock(&fork_m[l]);
            usleep(200);                               // даём соседям тоже взять левую вилку
            if (pthread_mutex_trylock(&fork_m[r]) == 0) return;
            pthread_mutex_unlock(&fork_m[l]);          // откат
            usleep(var == TRY_FIXED ? 500 : 100 + rand_r(&ph[i].seed) % 1000);
        }
    case WAITER:
        sem_wait(&waiter);
        pthread_mutex_lock(&fork_m[l]); pthread_mutex_lock(&fork_m[r]);
        break;
    }
}
static void put_forks(int i) {
    int l = i, r = (i + 1) % N;
    pthread_mutex_unlock(&fork_m[l]); pthread_mutex_unlock(&fork_m[r]);
    if (var == WAITER) sem_post(&waiter);
}

static void *philosopher(void *arg) {
    int i = (int)(long)arg;
    if (var == TRY_FIXED) pthread_barrier_wait(&start_bar);    // одновременный старт -> симметрия
    while (!stopped()) {
        usleep(rand_r(&ph[i].seed) % 1000);                     // думает
        long long t0 = now_ns();
        __atomic_store_n(&ph[i].hungry_since, t0, __ATOMIC_RELAXED);
        take_forks(i);
        if (stopped()) { if (var != TRY_FIXED && var != TRY_RAND) put_forks(i); break; }
        long long w = now_ns() - t0;
        if (w > ph[i].max_wait_ns) __atomic_store_n(&ph[i].max_wait_ns, w, __ATOMIC_RELAXED);
        __atomic_store_n(&ph[i].hungry_since, 0, __ATOMIC_RELAXED);
        usleep(1000);                                           // ест
        __atomic_add_fetch(&ph[i].meals, 1, __ATOMIC_RELAXED);
        put_forks(i);
    }
    return NULL;
}

static long total_meals(void) { long s = 0; for (int i = 0; i < N; i++) s += __atomic_load_n(&ph[i].meals, __ATOMIC_RELAXED); return s; }

int main(int argc, char **argv) {
    const char *names[] = {"naive", "ordered", "trylock-fixed", "trylock-rand", "waiter"};
    if (argc < 2) { fprintf(stderr, "usage: %s naive|ordered|trylock-fixed|trylock-rand|waiter [секунды]\n", argv[0]); return 1; }
    int found = 0; for (int v = 0; v < 5; v++) if (!strcmp(argv[1], names[v])) { var = v; found = 1; }
    if (!found) { fprintf(stderr, "неизвестный вариант\n"); return 1; }
    int secs = argc > 2 ? atoi(argv[2]) : 10, detect = !getenv("PHIL_NODETECT");
    for (int i = 0; i < N; i++) { pthread_mutex_init(&fork_m[i], NULL); ph[i].seed = 1234u + i; }
    sem_init(&waiter, 0, N - 1);
    pthread_barrier_init(&start_bar, NULL, N);
    pthread_t t[N];
    for (long i = 0; i < N; i++) pthread_create(&t[i], NULL, philosopher, (void *)i);
    printf("variant=%s pid=%d  (%ds)\n", names[var], getpid(), secs);
    fflush(stdout);

    long last = -1; int stalled = 0, elapsed = 0, verdict = 0;
    while (elapsed < secs || !detect) {
        sleep(1); elapsed++;
        long m = total_meals();
        if (detect && m == last) { if (++stalled >= 2) { verdict = 1; break; } } else stalled = 0;
        last = m;
    }
    __atomic_store_n(&stop_flag, 1, __ATOMIC_RELAXED);
    long long now = now_ns();
    printf("%s\n", verdict ? "!!! НЕТ ПРОГРЕССА 2 с: deadlock (потоки заблокированы) или livelock (потоки крутятся, еды нет)" : "работа завершена штатно");
    printf("философ  приёмов_пищи  макс_ожидание_мс\n");
    long tot = 0, mn = -1, mx = 0;
    for (int i = 0; i < N; i++) {
        long long w = __atomic_load_n(&ph[i].max_wait_ns, __ATOMIC_RELAXED), hs = __atomic_load_n(&ph[i].hungry_since, __ATOMIC_RELAXED);
        if (hs && now - hs > w) w = now - hs;                    // сейчас голодает — учитываем текущее ожидание
        long mm = __atomic_load_n(&ph[i].meals, __ATOMIC_RELAXED);
        printf("   %d      %8ld        %10.1f%s\n", i, mm, w / 1e6, hs ? "  (ещё голоден)" : "");
        tot += mm; if (mn < 0 || mm < mn) mn = mm; if (mm > mx) mx = mm;
    }
    printf("всего=%ld  min=%ld  max=%ld  время=%ds\n", tot, mn, mx, elapsed);
    fflush(stdout);
    if (verdict) _exit(2);           // при deadlock потоки не завершатся — не join'им
    for (int i = 0; i < N; i++) pthread_join(t[i], NULL);
    return 0;
}
