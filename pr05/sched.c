// Симулятор планирования: SRTF, RR, MLFQ (3 уровня, растущий квант, понижение при исчерпании кванта, периодический boost).
// ./sched --algo srtf|rr|mlfq [--quantum Q] [--boost B] [--gantt] file
// Входной файл: строки "PID arrival burst priority"; строки с # пропускаются.
// Соглашение RR/MLFQ: процесс, прибывший в момент t, ставится в очередь РАНЬШЕ процесса, вытесненного в тот же момент t.
// Равенство критерия в SRTF: раньше прибыл, при равенстве — меньший номер (порядок в файле); вытеснение только при строго меньшем остатке.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXN 256
#define LV 3
typedef struct { char name[16]; int arr, burst, prio, rem, start, fin, level, used; } P;
static P p[MAXN]; static int n;
static int q[LV][MAXN], qn[LV];
static char gantt[1 << 16]; static int gl;

static void push_back(int l, int i) { q[l][qn[l]++] = i; }
static void push_front(int l, int i) { memmove(&q[l][1], &q[l][0], qn[l] * sizeof(int)); q[l][0] = i; qn[l]++; }
static int pop_front(int l) { int v = q[l][0]; memmove(&q[l][0], &q[l][1], (--qn[l]) * sizeof(int)); return v; }

static void admit(int t) { for (int i = 0; i < n; i++) if (p[i].arr == t) push_back(0, i); }

static void log_run(int t, int who) {      // компактная диаграмма Ганта: "[t:имя]"
    static int last = -2;
    if (who != last && gl < (int)sizeof gantt - 40) gl += snprintf(gantt + gl, 40, "[%d:%s]", t, who < 0 ? "idle" : p[who].name);
    last = who;
}

static int load(const char *f) {
    FILE *fp = fopen(f, "r"); if (!fp) { perror(f); return -1; }
    char line[256];
    while (fgets(line, sizeof line, fp) && n < MAXN) {
        if (line[0] == '#' || line[0] == '\n') continue;
        P *x = &p[n];
        if (sscanf(line, "%15s %d %d %d", x->name, &x->arr, &x->burst, &x->prio) != 4) continue;
        x->rem = x->burst; x->start = -1; x->fin = -1; n++;
    }
    fclose(fp); return n ? 0 : -1;
}

static int simulate(const char *algo, int quantum, int boost) {
    int done = 0, t = 0, run = -1, used = 0, last = -1, ctx = 0, busy = 0;
    int is_srtf = !strcmp(algo, "srtf"), is_rr = !strcmp(algo, "rr"), is_mlfq = !strcmp(algo, "mlfq");
    int quant[LV] = {quantum, quantum * 2, quantum * 4};
    admit(0);
    while (done < n) {
        if (is_mlfq && boost > 0 && t > 0 && t % boost == 0) {       // периодический boost
            if (run >= 0) { push_back(0, run); run = -1; }
            for (int l = 1; l < LV; l++) while (qn[l]) push_back(0, pop_front(l));
            for (int i = 0; i < n; i++) p[i].level = 0;
        }
        if (is_srtf) {
            int best = -1;
            for (int i = 0; i < n; i++) if (p[i].arr <= t && p[i].rem > 0 &&
                (best < 0 || p[i].rem < p[best].rem || (p[i].rem == p[best].rem && (p[i].arr < p[best].arr)))) best = i;
            if (run < 0 || (best >= 0 && p[best].rem < p[run].rem)) run = best;
        } else {
            if (is_mlfq && run >= 0)                                  // вытеснение процессом более высокого уровня
                for (int l = 0; l < p[run].level; l++) if (qn[l]) { push_front(p[run].level, run); run = -1; break; }
            if (run < 0) {
                for (int l = 0; l < LV; l++) if (qn[l]) { run = pop_front(l); used = 0; break; }
            }
        }
        if (run < 0) { log_run(t, -1); t++; admit(t); continue; }      // простой CPU
        if (run != last) { if (last >= 0) ctx++; last = run; }
        if (p[run].start < 0) p[run].start = t;
        log_run(t, run);
        p[run].rem--; used++; busy++; t++;
        if (p[run].rem == 0) { p[run].fin = t; done++; run = -1; admit(t); continue; }
        if (is_srtf) { admit(t); continue; }
        admit(t);                                                      // сначала прибывшие, потом вытесненный
        int lim = is_rr ? quantum : quant[p[run].level];
        if (used >= lim) {
            int lv = p[run].level;
            if (is_mlfq && lv < LV - 1) lv++;
            p[run].level = lv; push_back(is_rr ? 0 : lv, run); run = -1;
        }
    }
    printf("%-6s  %-8s %-8s %-8s %-6s %-5s\n", "PID", "wait", "turn", "resp", "start", "fin");
    double sw = 0, st = 0, sr = 0;
    for (int i = 0; i < n; i++) {
        int turn = p[i].fin - p[i].arr, wait = turn - p[i].burst, resp = p[i].start - p[i].arr;
        printf("%-6s  %-8d %-8d %-8d %-6d %-5d\n", p[i].name, wait, turn, resp, p[i].start, p[i].fin);
        sw += wait; st += turn; sr += resp;
    }
    printf("\nalgo=%s%s  avg_wait=%.2f  avg_turnaround=%.2f  avg_response=%.2f  cpu_util=%.1f%%  ctx_switches=%d\n",
           algo, is_rr || is_mlfq ? "" : "", sw / n, st / n, sr / n, 100.0 * busy / t, ctx);
    return 0;
}

int main(int argc, char **argv) {
    const char *algo = "srtf", *file = NULL; int quantum = 0, boost = 50, show = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--algo") && i + 1 < argc) algo = argv[++i];
        else if (!strcmp(argv[i], "--quantum") && i + 1 < argc) quantum = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--boost") && i + 1 < argc) boost = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gantt")) show = 1;
        else file = argv[i];
    }
    if (!file || (strcmp(algo, "srtf") && strcmp(algo, "rr") && strcmp(algo, "mlfq"))) {
        fprintf(stderr, "usage: %s --algo srtf|rr|mlfq [--quantum Q] [--boost B] [--gantt] file\n", argv[0]); return 1; }
    if (quantum <= 0) quantum = !strcmp(algo, "mlfq") ? 4 : 3;
    if (load(file)) return 1;
    simulate(algo, quantum, boost);
    if (show) printf("\nGantt: %s\n", gantt);
    return 0;
}
