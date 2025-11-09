#include "builtins.h"

int try_builtin(command_t *c) {
    if (!c->argv || !c->argv[0]) return 0;

    if (strcmp(c->argv[0], "cd") == 0) {
        char *dir = c->argv[1] ? c->argv[1] : getenv("HOME");
        if (!dir) dir = "/";
        if (chdir(dir) != 0)
            perror("cd");
        return 1;
    }

    if (strcmp(c->argv[0], "exit") == 0) {
        int code = c->argv[1] ? atoi(c->argv[1]) : 0;
        exit(code);
    }

    return 0;
}
