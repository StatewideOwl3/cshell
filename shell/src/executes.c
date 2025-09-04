#include "../include/executes.h"

void executeCommand(struct shell_cmd* shellCommandStruct){
    // Check if the command is a 'hop' command
    if (strstr(shellCommandStruct->cmdGroupArr[0]->atomicArr[0]->atomicString, "hop") != NULL) {
        // Execute the hop command
        executeHop(shellCommandStruct);
        return;
    }
}