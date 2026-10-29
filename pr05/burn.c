// CPU-bound нагрузка: крутится, пока не получит SIGTERM. Печатает свой PID.
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
static volatile sig_atomic_t stop;
static void h(int s) { (void)s; stop = 1; }
int main(void) {
    signal(SIGTERM, h);
    volatile unsigned long x = 0;
    while (!stop) x++;
    return 0;
}
