#ifndef PARTE_H
#define PARTE_H

#include "parser.h"
#include "executes.h"


struct job {
    pid_t pid;
    char *command;   // store a copy of command string
    int running;     // 1 = running, 0 = stopped
    struct job* next;
};

extern struct job* job_list;

void addJob(pid_t pid, char* commandString, int running);

void removeJob(pid_t pid);

void updateJobs();

void printActivities();


void sendPing();


#endif