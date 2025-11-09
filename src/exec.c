#include "exec.h"
#include "builtins.h"

void execute_pipeline(command_t *cmds, int n) {
    if (n == 0) return;

    if (n == 1 && try_builtin(&cmds[0])) return;

    int (*pipes)[2] = NULL;
    if (n > 1) pipes = malloc(sizeof(int[2]) * (n - 1));

    for (int i = 0; i < n - 1; ++i)
        if (pipe(pipes[i]) < 0) { perror("pipe"); return; }

    pid_t *pids = calloc(n, sizeof(pid_t));

    for (int i = 0; i < n; ++i) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); continue; }

        if (pid == 0) {
            // child
            if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO);
            if (i < n - 1) dup2(pipes[i][1], STDOUT_FILENO);

            if (cmds[i].infile) {
                int fd = open(cmds[i].infile, O_RDONLY);
                if (fd < 0) { perror("open infile"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if (cmds[i].outfile) {
                int flags = O_CREAT | O_WRONLY | (cmds[i].append ? O_APPEND : O_TRUNC);
                int fd = open(cmds[i].outfile, flags, 0644);
                if (fd < 0) { perror("open outfile"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (pipes)
                for (int j = 0; j < n - 1; ++j) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

            if (!cmds[i].argv[0]) exit(0);
            execvp(cmds[i].argv[0], cmds[i].argv);
            fprintf(stderr, "shell: %s: %s\n", cmds[i].argv[0], strerror(errno));
            exit(127);
        } else {
            pids[i] = pid;
        }

        if (i > 0) close(pipes[i-1][0]);
        if (i < n - 1) close(pipes[i][1]);
    }

    if (pipes)
        for (int j = 0; j < n - 1; ++j) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }

    int status;
    for (int i = 0; i < n; ++i)
        if (pids[i] > 0) waitpid(pids[i], &status, 0);

    free(pipes);
    free(pids);
}
