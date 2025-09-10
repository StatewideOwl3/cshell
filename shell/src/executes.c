#include "../include/executes.h"
#include <string.h>

pid_t mainPid;

int bg_fork; // Global variable to indicate background process
int pipe_exists;

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
    for (int i = 0; i < num_atomics; i++) {
        struct atomic* atomicCmd = cmdGroupStruct->atomicArr[i];
        if (!atomicCmd || !atomicCmd->validity) continue;

        pids[i] = fork();
        if (pids[i] == 0) {
            // Child: Set up pipe connections
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
        } else if (pids[i] < 0) {
            perror("Fork failed");
            // Clean up already created pipes before returning
            for (int j = 0; j < num_atomics - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return;
        }
    }

    // Parent: Close all pipe ends
    for (int j = 0; j < num_atomics - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    // Wait for all children
    for (int i = 0; i < num_atomics; i++) {
        if (pids[i] > 0) {
            waitpid(pids[i], NULL, 0);
        }
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
        else if (!strcmp(cmd, "exit"))   exit(0);

    }
    else if (pipe_exists || bg_fork) {
        // We're already inside a forked child set up by the pipeline loop
        // -> just exec directly, no new fork
        //execvp("/bin/bash", (char*[]){"/bin/bash", "-c", atomicCmdStruct->atomicString, NULL});
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
            execvp(cmd, args);
            fprintf(stderr, "Command not found!\n");
            exit(1);
        } else {
            // Foreground standalone command: wait; do not add to activities list.
            waitpid(pid, NULL, 0);
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
