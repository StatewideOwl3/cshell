#ifndef EXECUTES_H
#define EXECUTES_H

#include "parser.h"
#include "partB.h"
#include "partE.h"

#include <sys/wait.h>
#include <fcntl.h>

extern pid_t mainPid;

extern int bg_fork; // Global variable to indicate background process
extern int pipe_exists;

#define MAX_BG_JOBS 100

struct bg_job {
    int job_num;
    pid_t pid;
    char* cmd_name;
    int status; // 0: running, 1: exited normally, 2: exited abnormally
};

extern struct bg_job bg_jobs[MAX_BG_JOBS];
extern int bg_job_count;
extern int next_job_num;

void add_bg_job(pid_t pid, char* cmd_name);
void check_bg_jobs();
void print_bg_job_status(int job_num, pid_t pid, char* cmd_name, int status);

void executeShellCommand(struct shell_cmd* shellCommandStruct);

void executeCmdGroup(struct cmd_group* cmdGroupStruct);

void executeAtomicCmd(struct atomic* atomicCmdStruct);

void executeActivities(struct atomic* atomicCmdStruct);

#endif // EXECUTES_H

