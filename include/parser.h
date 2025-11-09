#ifndef PARSER_H
#define PARSER_H
#include "common.h"

char **tokenize(const char *line, int *out_count);
command_t *parse_pipeline(char **tokens, int tokcount, int *out_ncmds);

#endif
