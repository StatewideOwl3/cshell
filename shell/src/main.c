#define MAX_INPUT_SIZE 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/utsname.h>

#include "../include/printPrompt.h"

int main(){
    // Store the directory path in which the shell is started in
    char* absolutePath = NULL; ; // POSIX 2001 can dynamically allocate memory for the path
    absolutePath = getcwd(absolutePath, 0);
    if (absolutePath== NULL){
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

    while(1){
        // Ready to accept commands:
        // Collect current working directory and username:
        char *currentPath = NULL;
        currentPath = getcwd(currentPath, 0);
        if (currentPath== NULL){
            perror("getcwd() error");
            exit(1);
        };
        
        char* pathToPrint = getPathToPrint(absolutePath, currentPath);
        
        printf("<%s@%s:%s>",username, sysinfo->sysname, pathToPrint);
        fflush(stdout); // Ensure the prompt is displayed immediately
        
        free(pathToPrint); // Free memory
        free(currentPath);

        // Take user command:
        char input[MAX_INPUT_SIZE];
        scanf("%4095[^\n]", input); // Limit input to prevent overflow
        getchar(); // Consume the newline character left by scanf else infinite prints (scanf does not consume it)

        // Process user command:
        printf("You entered: %s\n", input);
        
        // Repeat
    }
    return 0;
}