#define MAX_INPUT_SIZE 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/utsname.h>

#include "../include/printPrompt.h"
#include "../include/parser.h"
#include "../include/partB.h"
#include "../include/executes.h"

int main(){
    // Store the directory path in which the shell is started in 
    mainPid = getpid();
    absoluteHomePath = getcwd(NULL, 0);
    if (absoluteHomePath == NULL){
        perror("getcwd() error");
        exit(1);
    };

    char* username = getlogin();
    if (username == NULL){
        perror("getlogin() error");
        exit(1);
    }

    // Get the system name from uname struct
    struct utsname* sysinfo = (struct utsname*)malloc(sizeof(struct utsname));
    uname(sysinfo);

    loadLogs();

    while(1){
        // Check for completed background jobs
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
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL) continue;
        input[strcspn(input, "\n")] = 0; // Remove trailing newline
        if (strlen(input) == 0) continue; // Skip empty input


        // Verify user command:
        //printf("INPUT SCANNED: %s\n",input);
        struct shell_cmd* shellCmdStruct = verifyCommand(input);
        if (shellCmdStruct->validity == false){
            freeShellCmd(shellCmdStruct);
            continue;
        }
        
        // Add to log if not duplicate and not log command
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