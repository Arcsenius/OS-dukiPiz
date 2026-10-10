#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    char *name = NULL, *line = NULL, *reply = NULL;
    size_t name_cap = 0, line_cap = 0, reply_cap = 0;
    int to_child[2], from_child[2], rc = 0;
    printf("Output file: "); fflush(stdout);
    ssize_t n = getline(&name, &name_cap, stdin);
    if (n <= 1) { fprintf(stderr, "empty file name\n"); free(name); return 2; }
    if (name[n - 1] == '\n') name[n - 1] = '\0';
    if (pipe(to_child) == -1) { perror("pipe"); free(name); return 1; }
    if (pipe(from_child) == -1) { perror("pipe"); close(to_child[0]); close(to_child[1]); free(name); return 1; }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(to_child[0]); close(to_child[1]); close(from_child[0]); close(from_child[1]); free(name);
        return 1;
    }
    if (pid == 0) {
        close(to_child[1]); close(from_child[0]);
        if (dup2(to_child[0], STDIN_FILENO) == -1 || dup2(from_child[1], STDOUT_FILENO) == -1) { perror("dup2"); _exit(1); }
        close(to_child[0]); close(from_child[1]);
        execl("./sum_child", "sum_child", name, (char *)NULL);
        perror("execl"); _exit(127);
    }
    free(name);
    close(to_child[0]); close(from_child[1]);
    signal(SIGPIPE, SIG_IGN);
    FILE *send = fdopen(to_child[1], "w");
    FILE *recv = fdopen(from_child[0], "r");
    if (!send || !recv) {
        perror("fdopen");
        if (send) fclose(send); else close(to_child[1]);
        if (recv) fclose(recv); else close(from_child[0]);
        free(line); free(reply);
        waitpid(pid, NULL, 0);
        return 1;
    }
    printf("Enter integer lines (Ctrl-D to finish):\n");
    while ((n = getline(&line, &line_cap, stdin)) != -1) {
        if (fwrite(line, 1, (size_t)n, send) != (size_t)n ||
            (line[n - 1] != '\n' && fputc('\n', send) == EOF) || fflush(send) == EOF) {
            perror("send to child"); rc = 1; break;
        }
        if (getline(&reply, &reply_cap, recv) == -1) { fprintf(stderr, "child closed reply pipe\n"); rc = 1; break; }
        fputs(reply, stdout);
    }
    if (ferror(stdin)) { perror("read stdin"); rc = 1; }
    free(line); free(reply);
    if (fclose(send) == EOF && !rc) { perror("close pipe"); rc = 1; }
    fclose(recv);
    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) continue;
        perror("waitpid"); return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) rc = 1;
    return rc;
}
