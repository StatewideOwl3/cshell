#include "../include/hop.h"


const char* absoluteHomePath = NULL; // Global variable to hold the absolute home path

char* oldWD = NULL;

void executeHop(struct shell_cmd* shellCommandStruct){
    char* currentWD = getcwd(currentWD, 0);
    if (currentWD == NULL){
        perror("getcwd() error");
        return;
    }

    struct cmd_group* cmdGroup = shellCommandStruct->cmdGroupArr[0];
    struct atomic* atomicCmd = cmdGroup->atomicArr[0];
    char* atomicString = atomicCmd->atomicString;

    // Parse the atomicString and execute the command:
    char** executionArray = (char**)malloc(sizeof(atomicString) * sizeof(char*)); // Allocate memory for  arguments

    
}