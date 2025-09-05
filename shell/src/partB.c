#include "../include/partB.h"


char* absoluteHomePath = NULL; // Global variable to hold the absolute home path

char* oldWD = NULL;

void executeHop(struct shell_cmd* shellCommandStruct){
    //printf("entered executeHop\n");
    char* currentWD = getcwd(NULL, 0);
    if (currentWD == NULL){
        perror("getcwd() error");
        return;
    }

    struct cmd_group* cmdGroup = shellCommandStruct->cmdGroupArr[0];
    struct atomic* atomicCmd = cmdGroup->atomicArr[0];
    struct terminal* terminalCmd = atomicCmd->terminalArr[0];
    int argCount = terminalCmd->cmdAndArgsIndex;
    char** args = terminalCmd->cmdAndArgs;

    if (argCount < 2) {
        // go to home directory
        if (chdir(absoluteHomePath) != 0) {
            perror("hop: chdir to home directory failed");
            free(currentWD);
            return;
        }
    }

    for (int i = 1; i < argCount; i++) {
        if (strcmp(args[i], "-") == 0) {
            if (oldWD == NULL) {
                //perror("hop: OLDPWD not set\n");
                free(currentWD);
                return;
            }
            if (chdir(oldWD) != 0) {
                //perror("hop: chdir to OLDPWD failed\n");
                chdir(currentWD); // revert to current directory
                free(currentWD);
                return;
            }
            //printf("%s\n", oldWD);
        } 
        else if (strcmp(args[i], ".")==0){
            // stay in the same directory
            continue;
        }
        else if (strcmp(args[i], "..")==0){
            // go to parent directory
            if (chdir("..") != 0) {
                //perror("hop: chdir to parent directory failed\n");
                chdir(currentWD); // revert to current directory
                free(currentWD);
                return;
            }
        }
        else if (strcmp(args[i], "~")==0){
            // go to home directory
            if (chdir(absoluteHomePath) != 0) {
                perror("hop: chdir to home directory failed");
                chdir(currentWD); // revert to current directory
                free(currentWD);
                return;
            }
        }
        else if (args[i][0] == '/') {
            // absolute path, go to specified directory
            if (chdir(args[i]) != 0) {
                printf("No such directory!\n");
                chdir(currentWD); // revert to current directory
                free(currentWD);
                return;
            }
        }
        else {
            // go to specified directory
            if (chdir(args[i]) != 0) {
                printf("No such directory!\n");
                chdir(currentWD); // revert to current directory
                free(currentWD);
                return;
            }
        }
        // Update oldWD after a successful directory change
        free(oldWD);
        oldWD = currentWD;
        currentWD = getcwd(NULL, 0);
        if (currentWD == NULL){
            perror("getcwd() error");
            return;
        }
    }

}


void executeReveal(struct shell_cmd* shellCommandStruct){
    struct cmd_group* cmdGroup = shellCommandStruct->cmdGroupArr[0];
    struct atomic* atomicGroup = cmdGroup->atomicArr[0];
    struct terminal* terminalCmd = atomicGroup->terminalArr[0];
    int argCount = terminalCmd->cmdAndArgsIndex;
    char** args = terminalCmd->cmdAndArgs;

    if (argCount > 4) {
        printf("reveal: Invalid Syntax!\n");
        return;
    }

    int allFlag = 0;
    int lineFlag = 0;

    char* dirPath = NULL;

    for (int i = 1; i < argCount; i++) {
        if (strcmp(args[i], "-a") == 0) {
            allFlag = 1;
        } 
        else if (strcmp(args[i], "-l") == 0) {
            lineFlag = 1;
        }
        else if (strcmp(args[i], "-al") == 0 || strcmp(args[i], "-la") == 0) {
            allFlag = 1;
            lineFlag = 1;
        }

        else if (strcmp(args[i], "-") == 0) {
            if (oldWD == NULL) {
                printf("No such directory!\n");
                return;
            }
            dirPath = oldWD;
        }
        else if (strcmp(args[i], "~") == 0) {
            dirPath = absoluteHomePath;
        }
        else if (strcmp(args[i], ".") == 0) {
            dirPath = getcwd(NULL, 0);
            if (dirPath == NULL) {
                perror("getcwd() error");
                return;
            }
        }
        else if (strcmp(args[i], "..") == 0) {
            dirPath = realpath("..", NULL);
            if (dirPath == NULL) {
                perror("realpath() error");
                return;
            }
        }
        else {
            dirPath = args[i];
        }
    }

    if (dirPath == NULL)
        dirPath = absoluteHomePath;

    DIR* dir;
    struct dirent* entry;

    dir = opendir(dirPath);
    if (dir == NULL) {
        printf("No such directory!\n");
        return;
    }

    while((entry = readdir(dir)) != NULL) {
        // Skip hidden files unless -a flag is set
        if (!allFlag && entry->d_name[0] == '.') {
            continue;
        }

        if (lineFlag) {
            // Print detailed information
            printf("%s\n", entry->d_name);
        } else {
            // Print just the names
            printf("%s  ", entry->d_name);
        }
    }
    if (!lineFlag) {
        printf("\n");
    }
    closedir(dir);
    return;
}