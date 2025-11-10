#ifndef HISTORY_H
#define HISTORY_H

#define HISTORY_MAX 1000
#define HISTORY_PATH ".shell_history"

void history_init(void);
void history_add(const char *line);
const char *history_prev(void);
const char *history_next(void);
void history_save(void);
void history_cleanup(void);

#endif
