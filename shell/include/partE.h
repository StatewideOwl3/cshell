#ifndef PARTE_H
#define PARTE_H

#include "parser.h"
#include "executes.h"


struct job {
    pid_t pid;
    char *command;   // store a copy of command string
    int running;     // 1 = running, 0 = stopped
    struct job* nextJob;
};

extern struct job* jobListHead;
extern struct job* jobListTail;



#endif