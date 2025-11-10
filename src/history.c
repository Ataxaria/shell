#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *history[HISTORY_MAX];
static int history_count = 0;
static int history_index = -1;

void history_init(void) {
    FILE *f = fopen(HISTORY_PATH, "r");
    if (!f) return;

    char *line = NULL;
    size_t len = 0;
    size_t nread;
    while ((nread = getline(&line, &len, f)) != -1) {
        if (nread > 0 && line[nread - 1] == '\n')
            line[nread - 1] = '\0';
        if (history_count < HISTORY_MAX)
            history[history_count++] = strdup(line);
    }
    free(line);
    fclose(f);
    history_index = history_count;
}

void history_add(const char *line) {
    if (!line || !*line) return;
    if (history_count < HISTORY_MAX)
        history[history_count++] = strdup(line);
    else {
        free(history[0]);
        memmove(&history[0], &history[1], sizeof(char *) * (HISTORY_MAX - 1));
        history[HISTORY_MAX - 1] = strdup(line);
    }
    history_index = history_count;
}

const char *history_prev(void) {
    if (history_count == 0) return NULL;
    if (history_index > 0) history_index--;
    return history[history_index];
}

const char *history_next(void) {
    if (history_count == 0) return NULL;
    if (history_index < history_count - 1) {
        history_index++;
        return history[history_index];
    } else {
        history_index = history_count;
        return "";
    }
}

void history_save(void) {
    FILE *f = fopen(HISTORY_PATH, "w");
    if (!f) return;
    for (int i = 0; i < history_count; i++)
        fprintf(f, "%s\n", history[i]);
    fclose(f);
}

void history_cleanup(void) {
    for (int i = 0; i < history_count; i++)
        free(history[i]);
}
