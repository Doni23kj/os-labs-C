// Мини-shell: fork+execvp, pipe, <, >, &, cd, exit, jobs, fg, SIGCHLD, Ctrl+C.
// Простой разбор: токены разделяются пробелами; | < > & распознаются и без пробелов; "двойные кавычки" поддерживаются.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>

#define MAXTOK 128
#define MAXCMD 16
#define MAXJOB 64
#define MAXP   MAXCMD

typedef struct { char *argv[MAXTOK]; char *in, *out; } Cmd;
typedef struct {
    int used, id, fg, alive, stopped, np;
    pid_t pgid, pids[MAXP];
    char text[256];
} Job;

static Job jobs[MAXJOB];
static int next_id = 1;
static pid_t shell_pgid;
static int interactive;

/* ---------- SIGCHLD: уборка зомби ---------- */
static void on_sigchld(int sig) {
    (void)sig;
    int saved = errno, st;
    pid_t p;
    while ((p = waitpid(-1, &st, WNOHANG | WUNTRACED)) > 0) {
        for (int i = 0; i < MAXJOB; i++) {
            if (!jobs[i].used) continue;
            for (int k = 0; k < jobs[i].np; k++) {
                if (jobs[i].pids[k] != p) continue;
                if (WIFSTOPPED(st)) jobs[i].stopped = 1;
                else jobs[i].alive--;
            }
        }
    }
    errno = saved;
}

static Job *new_job(pid_t pgid, int fg, const char *text) {
    for (int i = 0; i < MAXJOB; i++) if (!jobs[i].used) {
        Job *j = &jobs[i];
        memset(j, 0, sizeof *j);
        j->used = 1; j->id = next_id++; j->pgid = pgid; j->fg = fg;
        snprintf(j->text, sizeof j->text, "%s", text);
        return j;
    }
    return NULL;
}

/* ---------- ожидание foreground-задания ---------- */
static void wait_fg(Job *j, sigset_t *prev) {
    if (interactive) tcsetpgrp(STDIN_FILENO, j->pgid);
    while (j->alive > 0 && !j->stopped) sigsuspend(prev);   // SIGCHLD заблокирован вне sigsuspend
    if (interactive) tcsetpgrp(STDIN_FILENO, shell_pgid);
    if (j->stopped && j->alive > 0) {
        j->fg = 0;
        printf("\n[%d] Stopped  %s\n", j->id, j->text);
    } else j->used = 0;
}

static void report_done(void) {      // вызывается при заблокированном SIGCHLD
    for (int i = 0; i < MAXJOB; i++)
        if (jobs[i].used && !jobs[i].fg && jobs[i].alive <= 0) {
            printf("[%d] Done     %s\n", jobs[i].id, jobs[i].text);
            jobs[i].used = 0;
        }
}

/* ---------- разбор ---------- */
static int tokenize(char *s, char **tok) {
    int n = 0;
    while (*s) {
        while (isspace((unsigned char)*s)) s++;
        if (!*s) break;
        if (n >= MAXTOK - 1) return -1;
        if (strchr("|<>&", *s)) { char *t = malloc(2); t[0] = *s++; t[1] = 0; tok[n++] = t; continue; }
        char buf[256]; int bi = 0, q = 0;
        while (*s && (q || (!isspace((unsigned char)*s) && !strchr("|<>&", *s)))) {
            if (*s == '"') { q = !q; s++; continue; }
            if (bi < (int)sizeof buf - 1) buf[bi++] = *s;
            s++;
        }
        buf[bi] = 0;
        tok[n++] = strdup(buf);
    }
    tok[n] = NULL;
    return n;
}

static int parse(char **tok, int n, Cmd *cmds, int *ncmd, int *bg) {
    *bg = 0; *ncmd = 0;
    if (n > 0 && !strcmp(tok[n - 1], "&")) { *bg = 1; n--; }
    Cmd *c = &cmds[0]; memset(c, 0, sizeof *c); int na = 0;
    for (int i = 0; i < n; i++) {
        if (!strcmp(tok[i], "|")) {
            if (na == 0 || *ncmd + 1 >= MAXCMD) return -1;
            c->argv[na] = NULL; (*ncmd)++; c = &cmds[*ncmd]; memset(c, 0, sizeof *c); na = 0;
        } else if (!strcmp(tok[i], "<") || !strcmp(tok[i], ">")) {
            if (i + 1 >= n || strchr("|<>&", tok[i + 1][0]) ) return -1;
            if (tok[i][0] == '<') c->in = tok[++i]; else c->out = tok[++i];
        } else if (!strcmp(tok[i], "&")) return -1;
        else c->argv[na++] = tok[i];
    }
    if (na == 0) return -1;
    c->argv[na] = NULL; (*ncmd)++;
    return 0;
}

/* ---------- запуск конвейера ---------- */
static void run_pipeline(Cmd *cmds, int n, int bg, const char *text) {
    sigset_t chld, prev;
    sigemptyset(&chld); sigaddset(&chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chld, &prev);          // пока заполняем таблицу, обработчик не вмешивается

    Job *job = new_job(0, !bg, text);
    if (!job) { fprintf(stderr, "слишком много заданий\n"); sigprocmask(SIG_SETMASK, &prev, NULL); return; }
    int prev_rd = -1;
    for (int i = 0; i < n; i++) {
        int fd[2] = {-1, -1};
        if (i < n - 1 && pipe(fd) < 0) { perror("pipe"); break; }
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); if (fd[0] >= 0) { close(fd[0]); close(fd[1]); } break; }
        if (pid == 0) {
            setpgid(0, job->pgid);                        // 0 для первой команды -> своя группа
            signal(SIGINT, SIG_DFL); signal(SIGTSTP, SIG_DFL);
            signal(SIGTTOU, SIG_DFL); signal(SIGTTIN, SIG_DFL); signal(SIGCHLD, SIG_DFL);
            sigprocmask(SIG_SETMASK, &prev, NULL);
            if (prev_rd >= 0) { dup2(prev_rd, 0); }
            if (fd[1] >= 0) { dup2(fd[1], 1); }
            if (prev_rd >= 0) close(prev_rd);             // закрываем ВСЕ лишние концы pipe,
            if (fd[0] >= 0) close(fd[0]);                 // иначе wc будет ждать EOF вечно
            if (fd[1] >= 0) close(fd[1]);
            if (cmds[i].in) {
                int f = open(cmds[i].in, O_RDONLY);
                if (f < 0) { perror(cmds[i].in); _exit(1); }
                dup2(f, 0); close(f);
            }
            if (cmds[i].out) {
                int f = open(cmds[i].out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (f < 0) { perror(cmds[i].out); _exit(1); }
                dup2(f, 1); close(f);
            }
            execvp(cmds[i].argv[0], cmds[i].argv);
            fprintf(stderr, "%s: %s\n", cmds[i].argv[0], strerror(errno));
            _exit(127);
        }
        if (job->pgid == 0) job->pgid = pid;
        setpgid(pid, job->pgid);                          // дублируем в родителе (гонка с ребёнком)
        job->pids[job->np++] = pid; job->alive++;
        if (prev_rd >= 0) close(prev_rd);
        if (fd[1] >= 0) close(fd[1]);                     // в родителе закрываем write-конец
        prev_rd = fd[0];
    }
    if (prev_rd >= 0) close(prev_rd);
    if (job->np == 0) { job->used = 0; sigprocmask(SIG_SETMASK, &prev, NULL); return; }

    if (bg) printf("[%d] %d\n", job->id, job->pgid);
    else wait_fg(job, &prev);
    sigprocmask(SIG_SETMASK, &prev, NULL);
}

/* ---------- встроенные команды ---------- */
static Job *find_job(int id) {
    for (int i = 0; i < MAXJOB; i++) if (jobs[i].used && jobs[i].id == id) return &jobs[i];
    return NULL;
}
static int builtin(Cmd *c) {
    char **a = c->argv;
    if (!strcmp(a[0], "exit")) exit(a[1] ? atoi(a[1]) : 0);
    if (!strcmp(a[0], "cd")) {
        const char *d = a[1] ? a[1] : getenv("HOME");
        if (!d || chdir(d) < 0) perror("cd");
        return 1;
    }
    if (!strcmp(a[0], "jobs")) {
        for (int i = 0; i < MAXJOB; i++) if (jobs[i].used && !jobs[i].fg)
            printf("[%d] %s  %s\n", jobs[i].id, jobs[i].stopped ? "Stopped" : "Running", jobs[i].text);
        return 1;
    }
    if (!strcmp(a[0], "fg") || !strcmp(a[0], "bg")) {
        sigset_t chld, prev; sigemptyset(&chld); sigaddset(&chld, SIGCHLD);
        sigprocmask(SIG_BLOCK, &chld, &prev);
        Job *j = NULL;
        if (a[1]) j = find_job(atoi(a[1][0] == '%' ? a[1] + 1 : a[1]));
        else for (int i = MAXJOB - 1; i >= 0; i--) if (jobs[i].used && !jobs[i].fg) { j = &jobs[i]; break; }
        if (!j) fprintf(stderr, "%s: нет такого задания\n", a[0]);
        else {
            j->stopped = 0;
            kill(-j->pgid, SIGCONT);
            if (a[0][0] == 'f') { j->fg = 1; wait_fg(j, &prev); }
        }
        sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;
    }
    return 0;
}

int main(void) {
    interactive = isatty(STDIN_FILENO);
    signal(SIGINT, SIG_IGN);      // Ctrl+C не должен убивать оболочку; дочерние сбрасывают в SIG_DFL
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN); signal(SIGTTIN, SIG_IGN);
    struct sigaction sa = {0};
    sa.sa_handler = on_sigchld; sa.sa_flags = SA_RESTART; sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
    if (interactive) { shell_pgid = getpid(); setpgid(shell_pgid, shell_pgid); tcsetpgrp(STDIN_FILENO, shell_pgid); }

    char *line = NULL; size_t cap = 0;
    for (;;) {
        sigset_t chld, prev; sigemptyset(&chld); sigaddset(&chld, SIGCHLD);
        sigprocmask(SIG_BLOCK, &chld, &prev); report_done(); sigprocmask(SIG_SETMASK, &prev, NULL);
        if (interactive) { printf("msh> "); fflush(stdout); }
        ssize_t r = getline(&line, &cap, stdin);
        if (r < 0) { if (interactive) puts(""); break; }
        if (line[r - 1] == '\n') line[r - 1] = 0;

        char text[256]; snprintf(text, sizeof text, "%s", line);
        char *tok[MAXTOK]; int n = tokenize(line, tok);
        if (n < 0) { fprintf(stderr, "слишком много токенов\n"); continue; }
        if (n == 0) continue;
        Cmd cmds[MAXCMD]; int nc, bg;
        if (parse(tok, n, cmds, &nc, &bg) < 0) { fprintf(stderr, "синтаксическая ошибка\n"); }
        else if (nc == 1 && !bg && builtin(&cmds[0])) { /* встроенная команда выполнена */ }
        else run_pipeline(cmds, nc, bg, text);
        for (int i = 0; i < n; i++) free(tok[i]);
    }
    free(line);
    return 0;
}
