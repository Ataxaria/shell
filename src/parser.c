#include "parser.h"

#define MAX_TOKENS 1024

void free_command(command_t *c) {
    if (!c) return;
    if (c->argv) {
        for (char **p = c->argv; *p; ++p) free(*p);
        free(c->argv);
    }
    free(c->infile);
    free(c->outfile);
}

char **tokenize(const char *line, int *out_count) {
    char **tokens = calloc(MAX_TOKENS, sizeof(char*));
    int t = 0;
    const char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') ++p;
        if (!*p) break;

        if (t >= MAX_TOKENS - 1) break;

        if (*p == '\'' || *p == '"') {
            char quote = *p++;
            const char *start = p;
            size_t len = 0;
            while (*p && *p != quote) { ++p; ++len; }
            tokens[t++] = strndup(start, len);
            if (*p == quote) ++p;
        } else {
            const char *start = p;
            size_t len = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') { ++p; ++len; }
            tokens[t++] = strndup(start, len);
        }
    }

    tokens[t] = NULL;
    if (out_count) *out_count = t;
    return tokens;
}

command_t *parse_pipeline(char **tokens, int tokcount, int *out_ncmds) {
    command_t *cmds = calloc(tokcount + 1, sizeof(command_t));
    int ncmd = 0, i = 0;

    while (i < tokcount) {
        command_t cur = {0};
        char **argv_tmp = calloc(tokcount + 1, sizeof(char*));
        int argc = 0;

        while (i < tokcount && strcmp(tokens[i], "|") != 0) {
            if (strcmp(tokens[i], "<") == 0 && i + 1 < tokcount)
                cur.infile = strdup(tokens[++i]);
            else if ((strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0) && i + 1 < tokcount) {
                cur.append = (strcmp(tokens[i], ">>") == 0);
                cur.outfile = strdup(tokens[++i]);
            } else {
                argv_tmp[argc++] = strdup(tokens[i]);
            }
            ++i;
        }

        argv_tmp[argc] = NULL;
        cur.argv = argv_tmp;

        if (i < tokcount && strcmp(tokens[i], "|") == 0) ++i;
        cmds[ncmd++] = cur;
    }

    *out_ncmds = ncmd;
    return cmds;
}
