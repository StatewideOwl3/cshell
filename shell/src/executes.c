#include "../include/executes.h"
#include <string.h>
 #include <signal.h>
 #include <termios.h>
 #include <errno.h>

pid_t mainPid;

int bg_fork; // Global variable to indicate background process
int pipe_exists;

// Track the current job's process group when creating pipelines
pid_t current_job_pgid = -1;

struct bg_job* bg_job_head = NULL;
int next_job_num = 1;

int add_bg_job(pid_t pid, char* cmd_name) {
    struct bg_job* node = malloc(sizeof(struct bg_job));
    if (!node) return -1;
    node->job_num = next_job_num++;
    node->pid = pid;
    node->cmd_name = strdup(cmd_name ? cmd_name : "(null)");
    node->status = 0;
    node->next = bg_job_head;
    bg_job_head = node;
    return node->job_num;
}

void check_bg_jobs() {
    struct bg_job* prev = NULL;
    struct bg_job* cur = bg_job_head;
    while (cur) {
        if (cur->status == 0) { // only poll running jobs
            int status = 0;
            pid_t result = waitpid(cur->pid, &status, WNOHANG);
            if (result > 0) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    cur->status = 1; // exited normally
                } else {
                    cur->status = 2; // exited abnormally
                }
                print_bg_job_status(cur->job_num, cur->pid, cur->cmd_name, cur->status);
                // Remove node immediately after reporting
                struct bg_job* to_free = cur;
                if (prev) prev->next = cur->next; else bg_job_head = cur->next;
                cur = cur->next;
                free(to_free->cmd_name);
                free(to_free);
                continue; // skip prev advance
            }
        }
        prev = cur;
        cur = cur->next;
    }
}

void print_bg_job_status(int job_num, pid_t pid, char* cmd_name, int status) {
    if (status == 1) {
        printf("%s with pid %d exited normally\n", cmd_name, pid);
    } else if (status == 2) {
        printf("%s with pid %d exited abnormally\n", cmd_name, pid);
    }
}

int is_bg_job_running(pid_t pid) {
    struct bg_job* cur = bg_job_head;
    while (cur) {
        if (cur->pid == pid && cur->status == 0) return 1;
        cur = cur->next;
    }
    return 0;
}

void executeShellCommand(struct shell_cmd* shellCommandStruct){
    bg_fork = 0;
    pipe_exists = 0;
    if (!shellCommandStruct || shellCommandStruct->validity == false || shellCommandStruct->cmdArrIndex==0) return;
    int num_cmdGroups = shellCommandStruct->cmdArrIndex;
    for (int i = 0; i < num_cmdGroups; i++) {
        struct cmd_group* cmdGroup = shellCommandStruct->cmdGroupArr[i];
        if (cmdGroup && cmdGroup->validity) {
            // Check separator exists at this index and whether it's '&' for background
            if (i < shellCommandStruct->sepArrIndex && strcmp(shellCommandStruct->separatorArr[i], "&") == 0) {
                pid_t jobLeaderPid = fork();
                if (jobLeaderPid < 0) {
                    perror("Fork failed");
                } else if (jobLeaderPid == 0) {
                    // In child: set background flag and redirect stdin to /dev/null
                    bg_fork = 1;
                    // Create a new process group for the background job; child becomes group leader
                    setpgid(0, 0);
                    current_job_pgid = getpid();
                    // Reset default signal handling for the job
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    int devnull = open("/dev/null", O_RDONLY);
                    if (devnull >= 0) {
                        dup2(devnull, STDIN_FILENO);
                        close(devnull);
                    }
                    // Execute the command group
                    executeCmdGroup(cmdGroup);
                    exit(0); // Exit child process after execution
                } else {
                    // Parent: add to background jobs and print info
                    char* cmd_name = cmdGroup->cmdString ? cmdGroup->cmdString : "background job";
                    int job_num = add_bg_job(jobLeaderPid, cmd_name);
                    // Also add to shell job list (parent) so activities can see it
                    addJob(jobLeaderPid, cmd_name, 1);
                    if (job_num != -1) printf("[%d] %d\n", job_num, jobLeaderPid);
                    fflush(stdout);
                }
            } else {
                // Sequential execution: execute and block until done
                executeCmdGroup(cmdGroup);
            }
        }
    }

}

void executeCmdGroup(struct cmd_group* cmdGroupStruct) {
    if (!cmdGroupStruct || cmdGroupStruct->validity == false || cmdGroupStruct->atomicArrIndex == 0) return;
    int num_atomics = cmdGroupStruct->atomicArrIndex;

    // If only one atomic, no pipes needed so no need any more forks also
    if (num_atomics == 1) {
        executeAtomicCmd(cmdGroupStruct->atomicArr[0]);
        return;
    }

    // Create n-1 pipes for the pipeline
    int pipes[num_atomics - 1][2];
    pipe_exists = 1;
    for (int j = 0; j < num_atomics - 1; j++) {
        if (pipe(pipes[j]) == -1) {
            perror("pipe failed");
            return;  // Don't exit(1) here to avoid crashing the shell
        }
    }

    // Fork and set up each atomic in the pipeline (foreground only). We should NOT
    // register these children as background jobs for the activities list because
    // the user only expects background (&) jobs there. Foreground pipeline
    // children are waited on immediately below.
    pid_t pids[num_atomics];
    pid_t pgid = -1;
    for (int i = 0; i < num_atomics; i++) {
        struct atomic* atomicCmd = cmdGroupStruct->atomicArr[i];
        if (!atomicCmd || !atomicCmd->validity) continue;

        pids[i] = fork();
        if (pids[i] == 0) {
            // Child: Set up pipe connections
            // Put this child in the job's process group
            if (bg_fork) {
                // background: inherit group from job leader saved in current_job_pgid
                if (current_job_pgid > 0) setpgid(0, current_job_pgid);
            } else {
                // foreground pipeline: first child becomes group leader, others join
                if (i == 0) setpgid(0, 0); else setpgid(0, pgid > 0 ? pgid : getpid());
            }
            // Reset default signal handling in children
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            if (i > 0) {  // Not the first atomic: read from previous pipe
                // STDIN of this new child process is now set to pipe
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < num_atomics - 1) {  // Not the last atomic: write to next pipe
                // STDOUT of this new child process is now set to write of next pipe
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipe ends in child since STDIN
            for (int k = 0; k < num_atomics - 1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            // Execute the atomic
            executeAtomicCmd(atomicCmd);
            exit(0);  // Exit child after execution
        } else {
            // Parent: set up process group id for the pipeline
            if (!bg_fork) {
                if (i == 0) {
                    pgid = pids[0];
                    setpgid(pids[0], pgid);
                } else {
                    setpgid(pids[i], pgid);
                }
            }
        }
    }

    // Parent: Close all pipe ends
    for (int j = 0; j < num_atomics - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    // Foreground pipeline: give terminal to job and wait; Background already handled by caller
    if (!bg_fork) {
        // Hand terminal to the pipeline's process group
        if (isatty(STDIN_FILENO) && pgid > 0) tcsetpgrp(STDIN_FILENO, pgid);
        int status;
        for (int i = 0; i < num_atomics; i++) {
            if (pids[i] > 0) {
                waitpid(pids[i], &status, WUNTRACED);
            }
        }
        // Reclaim terminal control
        if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
    } else {
        // Background pipeline: nothing to wait here; job leader handles listing
    }
    // Clear pipe_exists flag after pipeline completes
    pipe_exists = 0;
}

void executeAtomicCmd(struct atomic* atomicCmdStruct) {
    if (!atomicCmdStruct || atomicCmdStruct->validity == 0 || atomicCmdStruct->termArrIndex == 0) return;

    struct terminal* firstTerm = atomicCmdStruct->terminalArr[0];
    if (!firstTerm || firstTerm->cmdAndArgsIndex == 0) return;
    char** args = firstTerm->cmdAndArgs;
    char* cmd = args[0];

    // --- Detect builtins ---
    int is_builtin = (!strcmp(cmd, "hop") || !strcmp(cmd, "reveal") 
    || !strcmp(cmd, "log") || !strcmp(cmd, "activities") || !strcmp(cmd, "ping")
    || !strcmp(cmd, "exit"));

    // --- Save original stdin/stdout for restoration ---
    int original_stdin  = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);
    if (original_stdin < 0 || original_stdout < 0) {
        perror("dup failed");
        return;
    }

    // --- Apply all redirections ---
    for (int i = 0; i < atomicCmdStruct->sepArrIndex - 1; i++) {
        char* sep = atomicCmdStruct->separatorArr[i];
        struct terminal* fnameTerm = fnameTerm = atomicCmdStruct->terminalArr[i+1];

        if (!sep || !fnameTerm || fnameTerm->cmdAndArgsIndex == 0) continue;
        char* filename = fnameTerm->cmdAndArgs[0];

        if (strcmp(sep, "<") == 0) {
            int fd_in = open(filename, O_RDONLY);
            if (fd_in < 0) { perror(""); goto restore; }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        } else if (strcmp(sep, ">") == 0) {
            int fd_out = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) { perror(""); goto restore; }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        } else if (strcmp(sep, ">>") == 0) {
            int fd_out = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd_out < 0) { perror(""); goto restore; }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        // ensures only last redirection of each type is applied
    }

    // --- Execute ---
    if (is_builtin) {
        // Builtins run directly in the current process (unless you want subshell semantics)
        if (!strcmp(cmd, "hop"))        executeHop(atomicCmdStruct);
        else if (!strcmp(cmd, "reveal")) executeReveal(atomicCmdStruct);
        else if (!strcmp(cmd, "log"))    executeLog(atomicCmdStruct);
        else if (!strcmp(cmd, "activities")) printActivities();
        else if (!strcmp(cmd, "ping"))   executePing(atomicCmdStruct);
        else if (!strcmp(cmd, "fg")) {
            if (firstTerm->cmdAndArgsIndex != 2) {
                fprintf(stderr, "Invalid syntax!\n");
            } else {
                char *end = NULL; long pidLong = strtol(args[1], &end, 10);
                if (end == args[1] || *end != '\0' || pidLong <= 0) {
                    fprintf(stderr, "Invalid syntax!\n");
                } else {
                    pid_t pid = (pid_t)pidLong;
                    pid_t pg = getpgid(pid);
                    if (pg < 0) { fprintf(stderr, "No such process found\n"); goto restore; }
                    // Give terminal to job's process group
                    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pg);
                    // Continue the job group
                    kill(-pg, SIGCONT);
                    // Wait for job leader to finish or stop
                    int status;
                    if (waitpid(pid, &status, WUNTRACED) < 0 && errno != ECHILD) {
                        // ignore
                    }
                    // Take back terminal
                    if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
                    // Update activities list based on status
                    if (WIFSTOPPED(status)) {
                        addJob(pid, atomicCmdStruct->atomicString ? atomicCmdStruct->atomicString : "job", 0);
                    } else {
                        removeJob(pid);
                    }
                }
            }
        }
        else if (!strcmp(cmd, "bg")) {
            if (firstTerm->cmdAndArgsIndex != 2) {
                fprintf(stderr, "Invalid syntax!\n");
            } else {
                char *end = NULL; long pidLong = strtol(args[1], &end, 10);
                if (end == args[1] || *end != '\0' || pidLong <= 0) {
                    fprintf(stderr, "Invalid syntax!\n");
                } else {
                    pid_t pid = (pid_t)pidLong;
                    pid_t pg = getpgid(pid);
                    if (pg < 0) { fprintf(stderr, "No such process found\n"); goto restore; }
                    // continue the background job group
                    if (kill(-pg, SIGCONT) == -1) {
                        fprintf(stderr, "No such process found\n");
                    } else {
                        // mark running in activities
                        addJob(pid, atomicCmdStruct->atomicString ? atomicCmdStruct->atomicString : "job", 1);
                    }
                }
            }
        }
        else if (!strcmp(cmd, "exit"))   exit(0);

    }
    else if (pipe_exists || bg_fork) {
        // We're already inside a forked child set up by the pipeline loop
        // -> just exec directly, no new fork
        //execvp("/bin/bash", (char*[]){"/bin/bash", "-c", atomicCmdStruct->atomicString, NULL});
        // Ensure default signal handling in exec'd process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        execvp(cmd, args);
        fprintf(stderr, "Command not found!\n");
    }
    else {
        // Standalone external command: fork + exec
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            goto restore;
        } else if (pid == 0) {
            //execvp("/bin/bash", (char*[]){"/bin/bash", "-c", atomicCmdStruct->atomicString, NULL});
            // Child: new process group for job-control
            setpgid(0, 0);
            // Foreground job should take default signal actions
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            execvp(cmd, args);
            fprintf(stderr, "Command not found!\n");
            exit(1);
        } else {
            // Parent: give terminal to child's process group and wait; on stop, keep in activities
            if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, pid);
            int status = 0;
            waitpid(pid, &status, WUNTRACED);
            if (isatty(STDIN_FILENO)) tcsetpgrp(STDIN_FILENO, getpgrp());
            if (WIFSTOPPED(status)) {
                // Add stopped foreground job to activities for control
                addJob(pid, atomicCmdStruct->atomicString ? atomicCmdStruct->atomicString : cmd, 0);
            }
        }
    }

    // --- Restore original FDs ---
restore:
    dup2(original_stdin, STDIN_FILENO);
    dup2(original_stdout, STDOUT_FILENO);
    close(original_stdin);
    close(original_stdout);
}


void executeActivities(struct atomic* atomicCmdStruct) {
    if (!atomicCmdStruct || atomicCmdStruct->validity == 0 || atomicCmdStruct->termArrIndex == 0) return;

    struct terminal* firstTerm = atomicCmdStruct->terminalArr[0];
    if (!firstTerm || firstTerm->cmdAndArgsIndex == 0) return;
    char** args = firstTerm->cmdAndArgs;
    char* cmd = args[0];

    if (strcmp(cmd, "activities") == 0) {
        printActivities();
    } else {
        fprintf(stderr, "Command not found!\n");
    }
}


void executePing(struct atomic* atomicCmdStruct){
    if (!atomicCmdStruct || atomicCmdStruct->validity == 0 || atomicCmdStruct->termArrIndex == 0) return;

    struct terminal* firstTerm = atomicCmdStruct->terminalArr[0];
    if (!firstTerm || firstTerm->cmdAndArgsIndex == 0) return;
    char** args = firstTerm->cmdAndArgs;
    (void)args[0]; // command name "ping" unused beyond presence

    // Expect exactly: ping <pid> <signal_number>
    if (firstTerm->cmdAndArgsIndex != 3) {
        fprintf(stderr, "Invalid syntax!\n");
        return;
    }

    char *end = NULL;
    long pidLong = strtol(args[1], &end, 10);
    if (end == args[1] || *end != '\0') { // non-numeric pid token
        fprintf(stderr, "Invalid syntax!\n");
        return;
    }
    if (pidLong <= 0) { // invalid / non-existing pid semantics
        fprintf(stderr, "No such process found\n");
        return;
    }

    end = NULL;
    long sigLong = strtol(args[2], &end, 10);
    if (end == args[2] || *end != '\0') { // non-numeric signal token
        fprintf(stderr, "Invalid syntax!\n");
        return;
    }

    if (sigLong < 0) { // treat negative signal as invalid syntax
        fprintf(stderr, "Invalid syntax!\n");
        return;
    }

    int actualSignal = (int)((sigLong % 32 + 32) % 32); // positive normalized modulo 32

    if (kill((pid_t)pidLong, actualSignal) == -1) {
        // Any failure per spec -> No such process found
        fprintf(stderr, "No such process found\n");
        return;
    }

    printf("Sent signal %d to process with pid %ld\n", actualSignal, pidLong);
    // Force an immediate reap/cleanup pass so activities reflects removal ASAP
    check_bg_jobs();
    updateJobs();
}
