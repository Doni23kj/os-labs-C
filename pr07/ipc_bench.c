// Передача данных между двумя процессами: pipe | mq | shm | sock.
//   ./ipc_bench METHOD TOTAL_BYTES MSG_SIZE
// Печатает: method,msg_size,total_bytes,seconds,MB_per_s,us_per_msg  (us_per_msg — среднее время на сообщение при потоковой передаче, не RTT)
// Очистка: SIGTERM/SIGINT -> shm_unlink, mq_unlink, завершение потомка.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <mqueue.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MQ_NAME  "/ipcbench_mq"
#define SHM_NAME "/ipcbench_shm"
#define SLOTS 8

static pid_t child;
static int created_mq, created_shm;

static void cleanup(void) {
    if (created_mq) mq_unlink(MQ_NAME);
    if (created_shm) shm_unlink(SHM_NAME);
    created_mq = created_shm = 0;
}
static void on_term(int s) {
    (void)s;
    if (child > 0) kill(child, SIGTERM);
    cleanup();
    _exit(128 + SIGTERM);
}
static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static void die(const char *m) { perror(m); cleanup(); exit(1); }

static void write_all(int fd, const char *b, size_t n) {
    while (n) { ssize_t w = write(fd, b, n); if (w < 0) { if (errno == EINTR) continue; die("write"); } b += w; n -= w; }
}
static size_t read_some(int fd, char *b, size_t n) {
    for (;;) { ssize_t r = read(fd, b, n); if (r >= 0) return r; if (errno != EINTR) die("read"); }
}

typedef struct { sem_t empty, full; size_t len[SLOTS]; } ShmHdr;

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s pipe|mq|shm|sock TOTAL_BYTES MSG_SIZE\n", argv[0]); return 1; }
    const char *method = argv[1];
    size_t total = strtoull(argv[2], NULL, 10), msg = strtoull(argv[3], NULL, 10);
    if (!msg || !total) return 1;
    struct sigaction sa = {0}; sa.sa_handler = on_term; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL);

    int fd[2] = {-1, -1}; mqd_t mq = (mqd_t)-1; ShmHdr *sh = NULL; char *slots = NULL; size_t mqmsg = msg;
    if (!strcmp(method, "pipe")) { if (pipe(fd) < 0) die("pipe"); }
    else if (!strcmp(method, "sock")) { if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0) die("socketpair"); }
    else if (!strcmp(method, "mq")) {
        long lim = 8192; FILE *f = fopen("/proc/sys/fs/mqueue/msgsize_max", "r");
        if (f) { if (fscanf(f, "%ld", &lim) != 1) lim = 8192; fclose(f); }
        if ((long)mqmsg > lim) { mqmsg = lim; fprintf(stderr, "# mq: msgsize_max=%ld, сообщение режется на куски по %ld Б\n", lim, lim); }
        struct mq_attr at = {0}; at.mq_maxmsg = 10; at.mq_msgsize = mqmsg;
        mq_unlink(MQ_NAME);
        mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0600, &at); if (mq == (mqd_t)-1) die("mq_open");
        created_mq = 1;
    } else if (!strcmp(method, "shm")) {
        size_t sz = sizeof(ShmHdr) + (size_t)SLOTS * msg;
        shm_unlink(SHM_NAME);
        int sf = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600); if (sf < 0) die("shm_open");
        created_shm = 1;
        if (ftruncate(sf, sz) < 0) die("ftruncate");
        sh = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, sf, 0); if (sh == MAP_FAILED) die("mmap");
        close(sf);
        slots = (char *)sh + sizeof(ShmHdr);
        sem_init(&sh->empty, 1, SLOTS); sem_init(&sh->full, 1, 0);          // pshared = 1
    } else { fprintf(stderr, "unknown method\n"); return 1; }

    char *buf = malloc(msg); if (!buf) die("malloc"); memset(buf, 'x', msg);
    size_t nmsg = (total + msg - 1) / msg;
    double t0 = now();

    child = fork();
    if (child < 0) die("fork");
    if (child == 0) {                                  /* ---------- получатель ---------- */
        size_t got = 0;
        if (fd[1] >= 0) close(fd[1]);                  // читатель закрывает write-конец, иначе EOF не придёт
        if (!strcmp(method, "pipe") || !strcmp(method, "sock")) {
            for (size_t r; (r = read_some(fd[0], buf, msg)) > 0; ) got += r;
        } else if (!strcmp(method, "mq")) {
            while (got < total) {
                ssize_t r = mq_receive(mq, buf, mqmsg, NULL); if (r < 0) { if (errno == EINTR) continue; die("mq_receive"); }
                got += r;
            }
        } else {
            size_t tail = 0;
            while (got < total) {
                sem_wait(&sh->full);
                size_t l = sh->len[tail]; memcpy(buf, slots + tail * msg, l);       // копируем из shm в локальный буфер
                got += l; tail = (tail + 1) % SLOTS;
                sem_post(&sh->empty);
            }
        }
        if (got != total) { fprintf(stderr, "получено %zu из %zu\n", got, total); _exit(3); }
        _exit(0);
    }
    /* ---------- отправитель ---------- */
    if (fd[0] >= 0) close(fd[0]);
    size_t sent = 0;
    if (!strcmp(method, "pipe") || !strcmp(method, "sock")) {
        while (sent < total) { size_t n = total - sent < msg ? total - sent : msg; write_all(fd[1], buf, n); sent += n; }
        close(fd[1]);                                  // EOF для получателя
    } else if (!strcmp(method, "mq")) {
        while (sent < total) { size_t n = total - sent < mqmsg ? total - sent : mqmsg;
            if (mq_send(mq, buf, n, 0) < 0) { if (errno == EINTR) continue; die("mq_send"); } sent += n; }
        nmsg = (total + mqmsg - 1) / mqmsg;
    } else {
        size_t head = 0;
        while (sent < total) {
            size_t n = total - sent < msg ? total - sent : msg;
            sem_wait(&sh->empty);
            memcpy(slots + head * msg, buf, n); sh->len[head] = n; head = (head + 1) % SLOTS;
            sem_post(&sh->full);
            sent += n;
        }
    }
    int st = 0; waitpid(child, &st, 0); child = 0;
    double dt = now() - t0;
    cleanup();
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { fprintf(stderr, "получатель завершился с ошибкой\n"); return 1; }
    printf("%s,%zu,%zu,%.3f,%.1f,%.3f\n", method, mqmsg, total, dt, total / 1e6 / dt, dt / nmsg * 1e6);
    free(buf);
    return 0;
}
