#include "../include/partB.h"

char* absoluteHomePath = NULL; // Global variable to hold the absolute home path

char* oldWD = NULL;

static int cmp(const void* a, const void* b) {
    return strcasecmp(*(char**)a, *(char**)b);
}


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
        fprintf(stderr, "reveal: Invalid Syntax!\n");
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
                //perror("getcwd() error");
                return;
            }
        }
        else if (strcmp(args[i], "..") == 0) {
            dirPath = realpath("..", NULL);
            if (dirPath == NULL) {
                //perror("realpath() error");
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
            //perror("getcwd() failed for CWD");
            return;
        }
        //printf("DEBUG: dirPath set to CWD: %s\n", dirPath);  // Add this for debugging
    }

    DIR* dir = opendir(dirPath);
    struct dirent* entry;
    if (dir == NULL) {
        //perror("opendir() failed");
        //printf("DEBUG: dirPath was: %s\n", dirPath);  // Add this
        free(dirPath);
        return;
    }

    // Collect entries
    char** names = NULL;
    int count = 0;
    int capacity = 10;
    names = malloc(capacity * sizeof(char*));
    if (!names) {
        perror("malloc failed");
        closedir(dir);
        free(dirPath);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden files unless -a flag is set
        if (!allFlag && entry->d_name[0] == '.') {
            continue;
        }
        if (count >= capacity) {
            capacity *= 2;
            names = realloc(names, capacity * sizeof(char*));
            if (!names) {
                perror("realloc failed");
                closedir(dir);
                free(dirPath);
                return;
            }
        }
        names[count++] = strdup(entry->d_name);
    }
    closedir(dir);

    // Sort case-insensitively
    qsort(names, count, sizeof(char*), cmp);

    // Print
    for (int i = 0; i < count; i++) {
        if (lineFlag) {
            printf("%s\n", names[i]);
        } else {
            printf("%s  ", names[i]);
        }
        free(names[i]);
    }
    if (!lineFlag) {
        printf("\n");
    }
    free(names);
    free(dirPath);
}

void executeLog(struct atomic* atomicCmd){
    struct terminal* terminalCmd = atomicCmd->terminalArr[0];
    int argCount = terminalCmd->cmdAndArgsIndex;
    char** args = terminalCmd->cmdAndArgs;

    if (argCount == 1) {
        // No arguments: print the log
        struct executedShellCommand* current = listHead;
        int count = 1;
        while (current != NULL) {
            printf("%s\n", current->shellCommandString);
            current = current->next;
        }
    } else if (argCount == 2 && strcmp(args[1], "purge") == 0) {
        // purge: clear the log
        struct executedShellCommand* current = listHead;
        while (current != NULL) {
            struct executedShellCommand* temp = current;
            current = current->next;
            free(temp->shellCommandString);
            free(temp);
        }
        listHead = NULL;
        listTail = NULL;
        logListSize = 0;
        saveLog();
    } else if (argCount == 3 && strcmp(args[1], "execute") == 0) {
        // execute <index>
        int index = atoi(args[2]);
        if (index < 1 || index > logListSize) {
            fprintf(stderr, "log: invalid index\n");
            return;
        }
        // Find the command: index 1 is newest (tail), index logListSize is oldest (head)
        int target = logListSize - index + 1; // 1-based from head
        struct executedShellCommand* current = listHead;
        for (int i = 1; i < target; i++) {
            current = current->next;
        }
        char* cmd = current->shellCommandString;
        // Execute without adding to log
        struct shell_cmd* shellCmdStruct = verifyCommand(cmd);
        if (shellCmdStruct->validity) {
            executeShellCommand(shellCmdStruct);
        }
        freeShellCmd(shellCmdStruct);
    } else {
        printf("log: invalid syntax\n");
    }
}

char* logFile = "/home/saikapilbharadwaj/Documents/OSN/mini-project-1-StatewideOwl3/shell/logs.txt";
int logListSize = 0;
struct executedShellCommand* listHead = NULL;
struct executedShellCommand* listTail = NULL;


void loadLogs(){
    char* path = (char*)malloc(strlen(absoluteHomePath) + strlen("/logs.txt") + 1);
    strcpy(path, absoluteHomePath);
    strcat(path, "/logs.txt");
    FILE* file = fopen(logFile, "r");
    if (file == NULL) {
        // If the file doesn't exist, it's not an error; just return
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove newline character if present
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }

        struct executedShellCommand* newLog = malloc(sizeof(struct executedShellCommand));
        newLog->shellCommandString = strdup(buffer);
        newLog->next = NULL;

        if (listHead == NULL) {
            // If the list is empty, the new log is both the head and tail
            listHead = newLog;
            listTail = newLog;
            logListSize++;
        } 
        else if (logListSize < 15) {
            // Otherwise, append the new log to the end of the list
            listTail->next = newLog;
            listTail = newLog;
            logListSize++;
        }
        else if (logListSize >= 15) {
            // Remove the oldest log (head) and append the new log to the end
            struct executedShellCommand* temp = listHead;
            listHead = listHead->next;
            free(temp->shellCommandString);
            free(temp);
            listTail->next = newLog;
            listTail = newLog;
        }
    }

    fclose(file);
}

void saveLog(){
    char* path = (char*)malloc(strlen(absoluteHomePath) + strlen("/logs.txt") + 1);
    strcpy(path, absoluteHomePath);
    strcat(path, "/logs.txt");
    FILE* file = fopen(logFile, "w+");
    if (file == NULL) {
        perror("Failed to open log file for writing");
        return;
    }

    struct executedShellCommand* current = listHead;
    while (current != NULL) {
        fprintf(file, "%s\n", current->shellCommandString);
        current = current->next;
    }

    fclose(file);
}

void addLog(char* commandString) {
    struct executedShellCommand* newLog = malloc(sizeof(struct executedShellCommand));
    newLog->shellCommandString = strdup(commandString);
    newLog->next = NULL;

    if (listHead == NULL) {
        // If the list is empty, the new log is both the head and tail
        listHead = newLog;
        listTail = newLog;
        logListSize++;
    } 
    else if (logListSize < 15) {
        // Otherwise, append the new log to the end of the list
        listTail->next = newLog;
        listTail = newLog;
        logListSize++;
    }
    else if (logListSize >= 15) {
        // Remove the oldest log (head) and append the new log to the end
        struct executedShellCommand* temp = listHead;
        listHead = listHead->next;
        free(temp->shellCommandString);
        free(temp);
        listTail->next = newLog;
        listTail = newLog;
    }
    saveLog(); // Save the updated log list to the file
}