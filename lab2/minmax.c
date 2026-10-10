#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const int *data;
    size_t begin, end;
    int min, max;
    unsigned hold_ms;
} Job;

static double seconds(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) { perror("clock_gettime"); exit(1); }
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void scan(Job *job) {
    int lo = INT_MAX, hi = INT_MIN;
    for (size_t i = job->begin; i < job->end; ++i) {
        int x = job->data[i];
        if (x < lo) lo = x;
        if (x > hi) hi = x;
    }
    job->min = lo;
    job->max = hi;
}

static void *worker(void *arg) {
    Job *job = arg;
    if (job->hold_ms) {
        struct timespec pause = { job->hold_ms / 1000, (job->hold_ms % 1000) * 1000000L };
        while (nanosleep(&pause, &pause) == -1 && errno == EINTR) {}
    }
    scan(job);
    return NULL;
}

static size_t positive_size(const char *text, const char *label) {
    if (text[0] == '-') { fprintf(stderr, "%s must be positive\n", label); exit(2); }
    errno = 0;
    char *end;
    uintmax_t n = strtoumax(text, &end, 10);
    if (errno || end == text || *end || n == 0 || n > SIZE_MAX) {
        fprintf(stderr, "invalid %s: %s\n", label, text); exit(2);
    }
    return (size_t)n;
}

int main(int argc, char **argv) {
    size_t n = 0, limit = 0, repeats = 5;
    unsigned hold_ms = 0;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) goto usage;
        if (!strcmp(argv[i], "-n")) n = positive_size(argv[i + 1], "array size");
        else if (!strcmp(argv[i], "-t")) limit = positive_size(argv[i + 1], "thread limit");
        else if (!strcmp(argv[i], "-r")) repeats = positive_size(argv[i + 1], "repeat count");
        else if (!strcmp(argv[i], "--hold-ms")) {
            size_t ms = positive_size(argv[i + 1], "hold time");
            if (ms > UINT_MAX) goto usage;
            hold_ms = (unsigned)ms;
        } else goto usage;
    }
    if (!n || !limit || n > SIZE_MAX / sizeof(int)) goto usage;
    if (limit > n) limit = n;
    if (limit > SIZE_MAX / sizeof(pthread_t) || limit > SIZE_MAX / sizeof(Job)) goto usage;
    int *data = malloc(n * sizeof(*data));
    pthread_t *threads = malloc(limit * sizeof(*threads));
    Job *jobs = calloc(limit, sizeof(*jobs));
    if (!data || !threads || !jobs) { perror("allocation"); free(data); free(threads); free(jobs); return 1; }
    uint32_t state = 123456789u;
    for (size_t i = 0; i < n; ++i) {
        state = 1664525u * state + 1013904223u;
        data[i] = (int)(state % 2000001u) - 1000000;
    }
    printf("pid=%ld, workers=%zu, n=%zu\n", (long)getpid(), limit, n);
    fflush(stdout);
    double serial_best = 1e99, parallel_best = 1e99;
    int serial_min = 0, serial_max = 0, parallel_min = 0, parallel_max = 0;
    for (size_t rep = 0; rep < repeats; ++rep) {
        Job baseline = { .data = data, .begin = 0, .end = n };
        double t0 = seconds();
        scan(&baseline);
        double dt = seconds() - t0;
        if (dt < serial_best) serial_best = dt;
        serial_min = baseline.min; serial_max = baseline.max;

        t0 = seconds();
        size_t created = 0;
        for (size_t i = 0; i < limit; ++i) {
            size_t chunk = n / limit, extra = n % limit;
            size_t begin = chunk * i + (i < extra ? i : extra);
            jobs[i] = (Job){ .data = data, .begin = begin,
                .end = begin + chunk + (i < extra), .hold_ms = hold_ms };
            int err = pthread_create(&threads[i], NULL, worker, &jobs[i]);
            if (err) { fprintf(stderr, "pthread_create: %s\n", strerror(err)); break; }
            ++created;
        }
        int failed = created != limit;
        for (size_t i = 0; i < created; ++i) {
            int err = pthread_join(threads[i], NULL);
            if (err) { fprintf(stderr, "pthread_join: %s\n", strerror(err)); failed = 1; }
        }
        if (failed) { free(data); free(threads); free(jobs); return 1; }
        dt = seconds() - t0;
        if (dt < parallel_best) parallel_best = dt;
        parallel_min = INT_MAX; parallel_max = INT_MIN;
        for (size_t i = 0; i < limit; ++i) {
            if (jobs[i].min < parallel_min) parallel_min = jobs[i].min;
            if (jobs[i].max > parallel_max) parallel_max = jobs[i].max;
        }
        if (parallel_min != serial_min || parallel_max != serial_max) {
            fprintf(stderr, "result mismatch\n"); free(data); free(threads); free(jobs); return 1;
        }
    }
    printf("min=%d max=%d\n", parallel_min, parallel_max);
    printf("serial=%.6f s parallel=%.6f s speedup=%.3f efficiency=%.3f\n",
           serial_best, parallel_best, serial_best / parallel_best,
           serial_best / parallel_best / limit);
    free(data); free(threads); free(jobs);
    return 0;
usage:
    fprintf(stderr, "usage: %s -n ARRAY_SIZE -t MAX_THREADS [-r REPEATS] [--hold-ms MILLISECONDS]\n", argv[0]);
    return 2;
}
