// Стоимость создания: pthread_create+join, fork+waitpid, clone(CLONE_VM)+waitpid. ./spawn_cost [ITER]
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static void *thr(void *a) { return a; }
static int clone_fn(void *a) { (void)a; return 0; }

int main(int argc, char **argv) {
    int N = argc > 1 ? atoi(argv[1]) : 10000;
    double t0 = now();
    for (int i = 0; i < N; i++) { pthread_t t; pthread_create(&t, NULL, thr, NULL); pthread_join(t, NULL); }
    double tt = now() - t0;

    t0 = now();
    for (int i = 0; i < N; i++) {
        pid_t p = fork();
        if (p == 0) _exit(0);
        waitpid(p, NULL, 0);
    }
    double tp = now() - t0;

    size_t ss = 64 * 1024;
    char *stack = mmap(NULL, ss, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    t0 = now();
    for (int i = 0; i < N; i++) {
        pid_t p = clone(clone_fn, stack + ss, CLONE_VM | SIGCHLD, NULL);   // общая память, но отдельный «процесс»
        if (p < 0) { perror("clone"); return 1; }
        waitpid(p, NULL, 0);
    }
    double tc = now() - t0;

    printf("# iterations=%d nproc=%ld\n", N, sysconf(_SC_NPROCESSORS_ONLN));
    printf("method,total_s,us_per_op\n");
    printf("pthread_create+join,%.4f,%.2f\n", tt, tt / N * 1e6);
    printf("fork+waitpid,%.4f,%.2f\n", tp, tp / N * 1e6);
    printf("clone(CLONE_VM)+waitpid,%.4f,%.2f\n", tc, tc / N * 1e6);
    return 0;
}
