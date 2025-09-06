#include "../include/partB.h"


char* absoluteHomePath = NULL; // Global variable to hold the absolute home path

char* oldWD = NULL;

void executeHop(struct atomic* atomicCmd){
    //printf("entered executeHop\n");
    char* currentWD = getcwd(NULL, 0);
    if (currentWD == NULL){
        perror("getcwd() error");
        return;
    }

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


bool checkRevealSyntax(struct atomic* atomicGroup) {
    struct terminal* terminalCmd = atomicGroup->terminalArr[0];
    int argCount = terminalCmd->cmdAndArgsIndex;
    char** args = terminalCmd->cmdAndArgs;

    // Must have at least the command "reveal"
    if (argCount < 1 || strcmp(args[0], "reveal") != 0) {
        return false;
    }

    int pathFound = 0;
    for (int i = 1; i < argCount; i++) {
        if (args[i][0] == '-') {
            // Check if it's a valid flag: - followed by a, l, or combinations
            for (int j = 1; args[i][j] != '\0'; j++) {
                if (args[i][j] != 'a' && args[i][j] != 'l') {
                    return false; // Invalid character in flag
                }
            }
        } else {
            // This should be the path, and only one path allowed
            if (pathFound) {
                return false; // Multiple paths
            }
            pathFound = 1;
            // Path can be ~, ., .., -, or any name (no validation on name here)
        }
    }
    return true;
}

void executeReveal(struct atomic* atomicGroup){
    struct terminal* terminalCmd = atomicGroup->terminalArr[0];
    int argCount = terminalCmd->cmdAndArgsIndex;
    char** args = terminalCmd->cmdAndArgs;

    if (!checkRevealSyntax(atomicGroup)) {
        printf("reveal: Invalid Syntax!\n");
        return;
    }

    int allFlag = 0;
    int lineFlag = 0;

    char* dirPath = NULL;

    for (int i = 1; i < argCount; i++) {
        if (args[i][0]=='-' && strstr(args[i], "a") != NULL) {
            allFlag = 1;
        } 
        if (args[i][0]=='-' && strstr(args[i], "l") != NULL) {
            lineFlag = 1;
        }
        if (args[i][0]=='-' && ((strstr(args[i], "al") != NULL || strstr(args[i], "la") != NULL))) {
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
        else if (args[i][0] != '-') {
            dirPath = args[i];
        }
    }

    if (dirPath == NULL){
        dirPath = getcwd(NULL, 0);
        if (dirPath == NULL) {
            perror("getcwd() failed for CWD");
            return;
        }
        //printf("DEBUG: dirPath set to CWD: %s\n", dirPath);  // Add this for debugging
    }

    DIR* dir = opendir(dirPath);
    struct dirent* entry;
    if (dir == NULL) {
        perror("opendir() failed");
        //printf("DEBUG: dirPath was: %s\n", dirPath);  // Add this
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

void executeLog(struct atomic* atomicCmd){
    // implement
}

