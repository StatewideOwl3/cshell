#define MAX_INPUT_SIZE 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <pwd.h>
 #include <signal.h>
 #include <termios.h>
 #include <errno.h>

#include "../include/printPrompt.h"
#include "../include/parser.h"
#include "../include/partB.h"
#include "../include/executes.h"
#include "../include/partE.h"



int main(){

    mainPid = getpid();
    // Put the shell in its own process group and grab the controlling terminal
    // Ignore failures quietly if not running interactively
    setpgid(0, 0);
    if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, getpgrp()); // Shell 
    }
    // Ignore interactive/tty signals in the shell; foreground jobs will receive them instead
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // Store the directory path in which the shell is started in 
    absoluteHomePath = getcwd(NULL, 0);
    if (absoluteHomePath == NULL){
        perror("getcwd() error");
        exit(1);
    };

    char* username = getlogin();
    if (username == NULL){
        struct passwd* pw = getpwuid(getuid());
        if (pw != NULL && pw->pw_name != NULL) {
            username = pw->pw_name;
        } else {
            username = "user";
        }
    }

    // Get the system name from uname struct
    struct utsname* sysinfo = (struct utsname*)malloc(sizeof(struct utsname));
    uname(sysinfo);

    loadLogs(); // Click to enter

    while(1){
        // Check for completed background jobs and print exit messages for them
        check_bg_jobs();

        // Ready to accept commands:
        // Collect current working directory and username:
        char *currentPath = NULL;
        currentPath = getcwd(NULL, 0);
        if (currentPath== NULL){
            perror("getcwd() error");
            exit(1);
        };
        
        char* pathToPrint = getPathToPrint(absoluteHomePath, currentPath);
        
        // Only print the prompt when running interactively (stdin is a terminal).
        if (isatty(STDIN_FILENO)){
            printf("<%s@%s:%s> ",username, sysinfo->nodename, pathToPrint);
            fflush(stdout); // Ensure the prompt is displayed immediately
        }
        
        free(pathToPrint); // Free memory
        free(currentPath);


        // Take user command:
        char input[MAX_INPUT_SIZE] = {0}; // Clearing it before reading into it
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) {
            // Handle EOF (Ctrl-D)
            if (feof(stdin)) {
                if (isatty(STDIN_FILENO)) printf("\n");
                // Send SIGKILL to all child processes/process groups
                kill_all_children();
                // Print logout and exit 0
                printf("logout\n");
                fflush(stdout);
                exit(0);
            }
            // Handle interrupted read
            if (ferror(stdin)) {
                if (errno == EINTR) { // Interrupted by signal; clear and continue
                    clearerr(stdin);
                    continue;
                }
                perror("input error");
                break;
            }
            continue;
        }
        input[strcspn(input, "\n")] = '\0'; // Remove trailing newline char
        if (strlen(input) == 0) continue; // Skip empty input


        // Verify user command:
        //printf("INPUT SCANNED: %s\n",input);
        struct shell_cmd* shellCmdStruct = verifyCommand(input);
        if (shellCmdStruct->validity == false){
            freeShellCmd(shellCmdStruct);
            continue;
        }
        
        // Add to log if not duplicate of last executed command and not log command
        if ((listTail == NULL || strcmp(input, listTail->shellCommandString) != 0) && strstr(input, "log") == NULL) {
            addLog(input);
        }
        
        // Process user command
        executeShellCommand(shellCmdStruct);

        // Free memory allocations at each level
        freeShellCmd(shellCmdStruct);
        
        // Repeat
    }
    return 0;
}