#include "../include/executes.h"

pid_t mainPid;

int bg_fork; // Global variable to indicate background process
int pipe_exists;

void executeShellCommand(struct shell_cmd* shellCommandStruct){
    if (!shellCommandStruct || shellCommandStruct->validity == 0 || shellCommandStruct->cmdArrIndex==0) return;
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
                    // In child: mark this as background and execute
                    bg_fork = 1; // Set background flag only in child
                    executeCmdGroup(cmdGroup);
                    exit(0); // Exit child process after execution
                }
                // Parent: continue to next command group (do not set bg_fork here)
            } else {
                // Sequential execution: execute and block until done
                executeCmdGroup(cmdGroup);
            }
        }
    }

}

void executeCmdGroup(struct cmd_group* cmdGroupStruct) {
    if (!cmdGroupStruct || cmdGroupStruct->validity == 0 || cmdGroupStruct->atomicArrIndex == 0) return;
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
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < num_atomics - 1) {  // Not the last atomic: write to next pipe
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipe ends in child
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
    int num_terminals = atomicCmdStruct->termArrIndex;

    // Get the command arguments from the first terminal
    struct terminal* firstTerm = atomicCmdStruct->terminalArr[0];
    if (!firstTerm || firstTerm->cmdAndArgsIndex == 0) return;
    char** args = firstTerm->cmdAndArgs;
    char* cmd = args[0];

    // Check if it's a built-in command
    int is_builtin = 0;
    if (strcmp(cmd, "hop") == 0 || strcmp(cmd, "reveal") == 0 || strcmp(cmd, "log") == 0) {
        is_builtin = 1;
    }

    // Save original stdin/stdout for restoration
    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);
    if (original_stdin < 0 || original_stdout < 0) {
        perror("dup failed");
        return;
    }

    // Set up redirections (for both built-ins and externals)
    for (int i = 0; i < num_terminals; i++) {
        struct terminal* term = atomicCmdStruct->terminalArr[i];
        if (!term || !term->validity || term->cmdAndArgsIndex == 0) continue;

        char* separator = (i < atomicCmdStruct->sepArrIndex) ? atomicCmdStruct->separatorArr[i] : NULL;
        if (separator) {
            if (strcmp(separator, "<") == 0 && i + 1 < num_terminals) {
                // Input redirection
                struct terminal* fnameTerm = atomicCmdStruct->terminalArr[i+1];
                if (!fnameTerm || fnameTerm->cmdAndArgsIndex == 0) {
                    fprintf(stderr, "Input redirection failed: missing filename token\n");
                    dup2(original_stdin, STDIN_FILENO);  // Restore
                    close(original_stdin);
                    close(original_stdout);
                    return;
                }
                // Diagnostic: print the filename token and raw terminal string
                if (fnameTerm->terminalString)
                    fprintf(stderr, "[DEBUG REDIR] input terminalString='%s'\n", fnameTerm->terminalString);
                if (fnameTerm->cmdAndArgs && fnameTerm->cmdAndArgs[0])
                    fprintf(stderr, "[DEBUG REDIR] input filename token='%s'\n", fnameTerm->cmdAndArgs[0]);
                int fd_in = open(fnameTerm->cmdAndArgs[0], O_RDONLY);
                if (fd_in < 0) {
                    perror("Input redirection failed");
                    dup2(original_stdin, STDIN_FILENO);  // Restore
                    close(original_stdin);
                    close(original_stdout);
                    return;
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            } else if (strcmp(separator, ">") == 0 && i + 1 < num_terminals) {
                // Output redirection (overwrite)
                struct terminal* outTerm = atomicCmdStruct->terminalArr[i+1];
                if (!outTerm || outTerm->cmdAndArgsIndex == 0) {
                    fprintf(stderr, "Output redirection failed: missing filename token\n");
                    dup2(original_stdout, STDOUT_FILENO);  // Restore
                    close(original_stdin);
                    close(original_stdout);
                    return;
                }
                int fd_out = open(outTerm->cmdAndArgs[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("Output redirection failed");
                    dup2(original_stdout, STDOUT_FILENO);  // Restore
                    close(original_stdin);
                    close(original_stdout);
                    return;
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            } else if (strcmp(separator, ">>") == 0 && i + 1 < num_terminals) {
                // Output redirection (append)
                struct terminal* outTerm2 = atomicCmdStruct->terminalArr[i+1];
                if (!outTerm2 || outTerm2->cmdAndArgsIndex == 0) {
                    fprintf(stderr, "Output redirection failed: missing filename token\n");
                    dup2(original_stdout, STDOUT_FILENO);  // Restore
                    close(original_stdin);
                    close(original_stdout);
                    return;
                }
                int fd_out = open(outTerm2->cmdAndArgs[0], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd_out < 0) {
                    perror("Output redirection failed");
                    dup2(original_stdout, STDOUT_FILENO);  // Restore
                    close(original_stdin);
                    close(original_stdout);
                    return;
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
        }
    }

    if (is_builtin) {
        // Execute built-in directly (no fork)
        if (strcmp(cmd, "hop") == 0) {
            executeHop(atomicCmdStruct);  // Pass the atomic struct or extract args as needed
        } else if (strcmp(cmd, "reveal") == 0) {
            executeReveal(atomicCmdStruct);
        } else if (strcmp(cmd, "log") == 0) {
            executeLog(atomicCmdStruct); 
        }
        // Restore original FDs
        dup2(original_stdin, STDIN_FILENO);
        dup2(original_stdout, STDOUT_FILENO);
        close(original_stdin);
        close(original_stdout);
    } else {
        // External command: Fork and exec
        if (bg_fork == 0 && pipe_exists == 0){
            pid_t pid = fork();
            if (pid < 0) {
                perror("Fork failed");
                // Restore FDs
                dup2(original_stdin, STDIN_FILENO);
                dup2(original_stdout, STDOUT_FILENO);
                close(original_stdin);
                close(original_stdout);
                return;
            } else if (pid == 0) {
                // Child: Execute the command (redirections are already set up)
                // set up arg registers and execvp
                execvp(cmd, args);
                // If execvp returns, there was an error
                perror("execvp failed");
                exit(1);
            } else {
                // Parent: Wait for child
                waitpid(pid, NULL, 0);
                // Restore FDs (though not strictly necessary for parent, good practice)
                dup2(original_stdin, STDIN_FILENO);
                dup2(original_stdout, STDOUT_FILENO);
                close(original_stdin);
                close(original_stdout);
            }
        }
        else{
            // just execvp
            execvp(cmd, args);
            // If execvp returns, there was an error
            perror("execvp failed");
            exit(1);
        }
    }
}