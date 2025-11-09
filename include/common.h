#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
    char **argv;
    char *infile;
    char *outfile;
    int append;
} command_t;

void free_command(command_t *c);

#endif
