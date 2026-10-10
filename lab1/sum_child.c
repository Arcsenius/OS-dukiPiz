#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s output.txt\n", argv[0]);
        return 2;
    }
    FILE *out = fopen(argv[1], "w");
    if (!out) { perror("fopen"); return 1; }
    char *line = NULL;
    size_t cap = 0;
    int rc = 0;
    while (getline(&line, &cap, stdin) != -1) {
        char *p = line;
        long long sum = 0;
        int count = 0, bad = 0;
        while (1) {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
            if (*p == '\0') break;
            errno = 0;
            char *end;
            long value = strtol(p, &end, 10);
            if (end == p || errno == ERANGE || value < INT_MIN || value > INT_MAX) { bad = 1; break; }
            if (*end != '\0' && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') { bad = 1; break; }
            if ((value > 0 && sum > LLONG_MAX - value) ||
                (value < 0 && sum < LLONG_MIN - value)) { bad = 1; break; }
            sum += value;
            ++count;
            p = end;
        }
        if (bad || count == 0) {
            if (puts("ERROR: expected int values") == EOF || fflush(stdout) == EOF) { rc = 1; break; }
            continue;
        }
        if (fprintf(out, "%lld\n", sum) < 0 || fflush(out) == EOF) { perror("write output"); rc = 1; break; }
        if (printf("OK: %lld\n", sum) < 0 || fflush(stdout) == EOF) { perror("write pipe"); rc = 1; break; }
    }
    if (ferror(stdin)) { perror("read pipe"); rc = 1; }
    free(line);
    if (fclose(out) == EOF) { perror("close output"); rc = 1; }
    return rc;
}
