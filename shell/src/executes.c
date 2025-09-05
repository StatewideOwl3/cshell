#include "../include/executes.h"

void executeCommand(struct shell_cmd* shellCommandStruct){
    // Check if the command is a 'hop' command
    if (shellCommandStruct->cmdGroupArr[0]->atomicArr[0]->terminalArr[0]->cmdAndArgsIndex > 0 &&
        strcmp(shellCommandStruct->cmdGroupArr[0]->atomicArr[0]->terminalArr[0]->cmdAndArgs[0], "hop") == 0) {
        // Execute the hop command
        executeHop(shellCommandStruct);
        return;
    }
    else if (shellCommandStruct->cmdGroupArr[0]->atomicArr[0]->terminalArr[0]->cmdAndArgsIndex > 0 && 
        strcmp(shellCommandStruct->cmdGroupArr[0]->atomicArr[0]->terminalArr[0]->cmdAndArgs[0], "reveal") == 0){
        executeReveal(shellCommandStruct);
        return;
    }
}