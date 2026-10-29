// Ограниченный буфер (5 элементов), 3 производителя, 2 потребителя, 1000 элементов.
//  ./bounded monitor   — монитор: структура + мьютекс + условные переменные
//  ./bounded sem       — два семафора + мьютекс
// Проверка: каждый элемент получен ровно один раз; число элементов в буфере всегда 0..5.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define CAP 5
#define TOTAL 1000
#define NP 3
#define NC 2

static int seen[TOTAL]; static int next_item;
static pthread_mutex_t seen_m = PTHREAD_MUTEX_INITIALIZER;
static int use_monitor;

/* ===== монитор ===== */
typedef struct { pthread_mutex_t m; pthread_cond_t not_full, not_empty; int buf[CAP], head, tail, count, closed; } Monitor;
static Monitor M = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, {0}, 0, 0, 0, 0 };
static void mon_put(int v) {
    pthread_mutex_lock(&M.m);
    while (M.count == CAP) pthread_cond_wait(&M.not_full, &M.m);        // while, не if!
    M.buf[M.tail] = v; M.tail = (M.tail + 1) % CAP; M.count++;
    if (M.count > CAP) { fprintf(stderr, "ПЕРЕПОЛНЕНИЕ\n"); exit(2); }
    pthread_cond_signal(&M.not_empty);
    pthread_mutex_unlock(&M.m);
}
static int mon_get(int *v) {
    pthread_mutex_lock(&M.m);
    while (M.count == 0 && !M.closed) pthread_cond_wait(&M.not_empty, &M.m);
    if (M.count == 0) { pthread_mutex_unlock(&M.m); return 0; }           // закрыт и пуст
    *v = M.buf[M.head]; M.head = (M.head + 1) % CAP; M.count--;
    pthread_cond_signal(&M.not_full);
    pthread_mutex_unlock(&M.m);
    return 1;
}
static void mon_close(void) { pthread_mutex_lock(&M.m); M.closed = 1; pthread_cond_broadcast(&M.not_empty); pthread_mutex_unlock(&M.m); }

/* ===== семафоры ===== */
static int sbuf[CAP], shead, stail, scount, sclosed;
static sem_t s_empty, s_full; static pthread_mutex_t s_m = PTHREAD_MUTEX_INITIALIZER;
static void sem_put(int v) {
    sem_wait(&s_empty);
    pthread_mutex_lock(&s_m); sbuf[stail] = v; stail = (stail + 1) % CAP; scount++; pthread_mutex_unlock(&s_m);
    sem_post(&s_full);
}
static int sem_get(int *v) {
    sem_wait(&s_full);
    pthread_mutex_lock(&s_m);
    if (sclosed && scount == 0) { pthread_mutex_unlock(&s_m); sem_post(&s_full); return 0; }   // «отравленная таблетка»
    *v = sbuf[shead]; shead = (shead + 1) % CAP; scount--; pthread_mutex_unlock(&s_m);
    sem_post(&s_empty);
    return 1;
}

static void *producer(void *a) {
    (void)a;
    for (;;) {
        pthread_mutex_lock(&seen_m); int v = next_item < TOTAL ? next_item++ : -1; pthread_mutex_unlock(&seen_m);
        if (v < 0) break;
        if (use_monitor) mon_put(v); else sem_put(v);
    }
    return NULL;
}
static void *consumer(void *a) {
    (void)a; int v;
    while (use_monitor ? mon_get(&v) : sem_get(&v)) {
        pthread_mutex_lock(&seen_m); seen[v]++; pthread_mutex_unlock(&seen_m);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2 || (strcmp(argv[1], "monitor") && strcmp(argv[1], "sem"))) { fprintf(stderr, "usage: %s monitor|sem\n", argv[0]); return 1; }
    use_monitor = !strcmp(argv[1], "monitor");
    if (!use_monitor) {
        if (sem_init(&s_empty, 0, CAP) != 0) { perror("sem_init(s_empty)"); return 1; }
        if (sem_init(&s_full, 0, 0) != 0) {
            perror("sem_init(s_full)");
            sem_destroy(&s_empty);
            return 1;
        }
    }
    pthread_t p[NP], c[NC];
    for (int i = 0; i < NC; i++) pthread_create(&c[i], NULL, consumer, NULL);
    for (int i = 0; i < NP; i++) pthread_create(&p[i], NULL, producer, NULL);
    for (int i = 0; i < NP; i++) pthread_join(p[i], NULL);
    if (use_monitor) mon_close(); else { pthread_mutex_lock(&s_m); sclosed = 1; pthread_mutex_unlock(&s_m); sem_post(&s_full); }
    for (int i = 0; i < NC; i++) pthread_join(c[i], NULL);
    if (!use_monitor) { sem_destroy(&s_empty); sem_destroy(&s_full); }
    int bad = 0; for (int i = 0; i < TOTAL; i++) if (seen[i] != 1) { bad++; fprintf(stderr, "элемент %d получен %d раз\n", i, seen[i]); }
    printf("%s: %s (%d нарушений)\n", argv[1], bad ? "ОШИБКА" : "OK, каждый из 1000 элементов получен ровно один раз", bad);
    return bad != 0;
}
