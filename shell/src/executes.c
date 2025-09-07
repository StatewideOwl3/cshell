#include "../include/executes.h"
#include <string.h>

pid_t mainPid;

int bg_fork; // Global variable to indicate background process
int pipe_exists;

struct bg_job bg_jobs[MAX_BG_JOBS];
int bg_job_count = 0;
int next_job_num = 1;

void add_bg_job(pid_t pid, char* cmd_name) {
    if (bg_job_count >= MAX_BG_JOBS) return;
    bg_jobs[bg_job_count].job_num = next_job_num++;
    bg_jobs[bg_job_count].pid = pid;
    bg_jobs[bg_job_count].cmd_name = strdup(cmd_name);
    bg_jobs[bg_job_count].status = 0; // running
    bg_job_count++;
}

void check_bg_jobs() {
    for (int i = 0; i < bg_job_count; i++) {
        if (bg_jobs[i].status == 0) {
            int status;
            pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
            if (result > 0) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    bg_jobs[i].status = 1; // exited normally
                } else {
                    bg_jobs[i].status = 2; // exited abnormally
                }
                print_bg_job_status(bg_jobs[i].job_num, bg_jobs[i].pid, bg_jobs[i].cmd_name, bg_jobs[i].status);
                bg_jobs[i].status = -1; // mark as processed
            }
        }
    }
}

void print_bg_job_status(int job_num, pid_t pid, char* cmd_name, int status) {
    if (status == 1) {
        printf("%s with pid %d exited normally\n", cmd_name, pid);
    } else if (status == 2) {
        printf("%s with pid %d exited abnormally\n", cmd_name, pid);
    }
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
                    add_bg_job(jobLeaderPid, cmd_name);
                    printf("[%d] %d\n", bg_jobs[bg_job_count-1].job_num, jobLeaderPid);
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

    // Fork and set up each atomic in the pipeline
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
}

void executeAtomicCmd(struct atomic* atomicCmdStruct) {
    if (!atomicCmdStruct || atomicCmdStruct->validity == 0 || atomicCmdStruct->termArrIndex == 0) return;

    struct terminal* firstTerm = atomicCmdStruct->terminalArr[0];
    if (!firstTerm || firstTerm->cmdAndArgsIndex == 0) return;
    char** args = firstTerm->cmdAndArgs;
    char* cmd = args[0];

    // --- Detect builtins ---
    int is_builtin = (!strcmp(cmd, "hop") || !strcmp(cmd, "reveal") || !strcmp(cmd, "log"));

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
    }
    else if (pipe_exists || bg_fork) {
        // We're already inside a forked child set up by the pipeline loop
        // -> just exec directly, no new fork
        //execvp("/bin/bash", (char*[]){"/bin/bash", "-c", atomicCmdStruct->atomicString, NULL});
        execvp(cmd, args);
        fprintf(stderr, "%s: command not found\n", cmd);
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
            fprintf(stderr, "%s: command not found\n",cmd);
            exit(1);
        } else {
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

    if (!strcmp(cmd, "hop")) {
        executeHop(atomicCmdStruct);
    } else if (!strcmp(cmd, "reveal")) {
        executeReveal(atomicCmdStruct);
    } else if (!strcmp(cmd, "log")) {
        executeLog(atomicCmdStruct);
    } else {
        fprintf(stderr, "%s: command not found\n", cmd);
    }
}

