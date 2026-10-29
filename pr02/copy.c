// Копирование файла разными способами. Использование:
//   ./copy MODE IN OUT [BUFSIZE]
// MODE: rw (read/write), stdio (fread/fwrite), sendfile, cfr (copy_file_range)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

static double now(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
static void die(const char *m) { perror(m); exit(1); }

static void copy_rw(const char *in, const char *out, size_t bs) {
    int fi = open(in, O_RDONLY), fo = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fi < 0 || fo < 0) die("open");
    char *buf = malloc(bs); if (!buf) die("malloc");
    ssize_t n;
    while ((n = read(fi, buf, bs)) > 0) {
        char *p = buf;
        while (n > 0) { ssize_t w = write(fo, p, n); if (w < 0) die("write"); p += w; n -= w; }
    }
    if (n < 0) die("read");
    free(buf); close(fi); close(fo);
}
static void copy_stdio(const char *in, const char *out, size_t bs) {
    FILE *fi = fopen(in, "rb"), *fo = fopen(out, "wb");
    if (!fi || !fo) die("fopen");
    char *buf = malloc(bs); if (!buf) die("malloc");
    size_t n;
    while ((n = fread(buf, 1, bs, fi)) > 0)
        if (fwrite(buf, 1, n, fo) != n) die("fwrite");
    free(buf); fclose(fi); fclose(fo);
}
static void copy_sendfile(const char *in, const char *out) {
    int fi = open(in, O_RDONLY), fo = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fi < 0 || fo < 0) die("open");
    struct stat st; fstat(fi, &st);
    off_t left = st.st_size;
    while (left > 0) { ssize_t n = sendfile(fo, fi, NULL, left > (1 << 30) ? (1 << 30) : left);
        if (n <= 0) die("sendfile");
        left -= n; }
    close(fi); close(fo);
}
static void copy_cfr(const char *in, const char *out) {
    int fi = open(in, O_RDONLY), fo = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fi < 0 || fo < 0) die("open");
    struct stat st; fstat(fi, &st);
    off_t left = st.st_size;
    while (left > 0) { ssize_t n = copy_file_range(fi, NULL, fo, NULL, left > (1 << 30) ? (1 << 30) : left, 0);
        if (n <= 0) die("copy_file_range");
        left -= n; }
    close(fi); close(fo);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s rw|stdio|sendfile|cfr IN OUT [BUFSIZE]\n", argv[0]); return 1; }
    const char *mode = argv[1];
    size_t bs = argc > 4 ? strtoul(argv[4], NULL, 10) : 4096;
    if (bs == 0) bs = 1;
    double t0 = now();
    if (!strcmp(mode, "rw")) copy_rw(argv[2], argv[3], bs);
    else if (!strcmp(mode, "stdio")) copy_stdio(argv[2], argv[3], bs);
    else if (!strcmp(mode, "sendfile")) copy_sendfile(argv[2], argv[3]);
    else if (!strcmp(mode, "cfr")) copy_cfr(argv[2], argv[3]);
    else { fprintf(stderr, "unknown mode\n"); return 1; }
    printf("%s,%zu,%.4f\n", mode, bs, now() - t0);   // mode,bufsize,seconds
    return 0;
}
