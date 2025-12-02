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

#define MAX_BG_JOBS 100  // no longer a hard storage cap; kept for legacy semantics

struct bg_job {
    int job_num;            // monotonically increasing ID
    pid_t pid;              // process ID
    char* cmd_name;         // duplicated command string
    int status;             // 0 running, 1 exited normally, 2 exited abnormally (used transiently)
    struct bg_job* next;    // singly-linked list
};

extern struct bg_job* bg_job_head;
extern int next_job_num;

// Add a background job; returns the assigned job number or -1 on failure.
int add_bg_job(pid_t pid, char* cmd_name);
void check_bg_jobs();
void print_bg_job_status(int job_num, pid_t pid, char* cmd_name, int status);

// Helper to let other modules know if a PID corresponds to a still-running
// background job tracked by the bg_jobs array (status == 0).
int is_bg_job_running(pid_t pid);

void executeShellCommand(struct shell_cmd* shellCommandStruct);

void executeCmdGroup(struct cmd_group* cmdGroupStruct);

void executeAtomicCmd(struct atomic* atomicCmdStruct);

void executeActivities(struct atomic* atomicCmdStruct);

void executePing(struct atomic* atomicCmdStruct);

// Kill all known child jobs/process groups (used on EOF/Ctrl-D)
void kill_all_children(void);

#endif // EXECUTES_H
