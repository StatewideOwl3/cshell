#include "../include/partB.h"

char* absoluteHomePath = NULL; // Global variable to hold the absolute home path

char* oldWD = NULL;
char* logFile = NULL;

static const char* getLogFilePath(void) {
    if (logFile != NULL) return logFile;
    if (absoluteHomePath == NULL) return NULL;

    size_t len = strlen(absoluteHomePath) + strlen("/logs.txt") + 1;
    logFile = malloc(len);
    if (logFile == NULL) return NULL;
    strcpy(logFile, absoluteHomePath);
    strcat(logFile, "/logs.txt");
    return logFile;
}

static int refresh_directory_state(char** currentWD) {
    char* updated = getcwd(NULL, 0);
    if (updated == NULL) {
        perror("getcwd() error");
        return -1;
    }
    free(oldWD);
    oldWD = *currentWD;
    *currentWD = updated;
    return 0;
}

static int cmp(const void* a, const void* b) {
    return strcmp(*(char**)a, *(char**)b);
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
        if (chdir(absoluteHomePath) != 0) {
            perror("hop: chdir to home directory failed");
            free(currentWD);
            return;
        }
        if (refresh_directory_state(&currentWD) != 0) {
            free(currentWD);
            return;
        }
    }

    for (int i = 1; i < argCount; i++) {
        int changed = 0;
        if (strcmp(args[i], "-") == 0) {
            if (oldWD == NULL) {
                printf("No such directory!\n");
                continue;
            }
            if (chdir(oldWD) != 0) {
                printf("No such directory!\n");
                continue;
            }
            changed = 1;
        } 
        else if (strcmp(args[i], ".")==0){
            // stay in the same directory
            continue;
        }
        else if (strcmp(args[i], "..")==0){
            if (chdir("..") != 0) {
                printf("No such directory!\n");
                continue;
            }
            changed = 1;
        }
        else if (strcmp(args[i], "~")==0){
            if (chdir(absoluteHomePath) != 0) {
                perror("hop: chdir to home directory failed");
                free(currentWD);
                return;
            }
            changed = 1;
        }
        else {
            if (chdir(args[i]) != 0) {
                printf("No such directory!\n");
                continue;
            }
            changed = 1;
        }

        if (changed) {
            if (refresh_directory_state(&currentWD) != 0) {
                free(currentWD);
                return;
            }
        }
    }

    free(currentWD);
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

    // Flags to indicate display formats..?
    int allFlag = 0;
    int lineFlag = 0;

    char* dirPath = NULL;
    int dirPath_allocated = 0; // 1 if dirPath should be freed

    // Set flags
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
            dirPath_allocated = 0;
        }
        else if (strcmp(args[i], "~") == 0) {
            dirPath = absoluteHomePath;
            dirPath_allocated = 0;
        }
        else if (strcmp(args[i], ".") == 0) {
            dirPath = getcwd(NULL, 0);
            if (dirPath == NULL) {
                //perror("getcwd() error");
                return;
            }
            dirPath_allocated = 1;
        }
        else if (strcmp(args[i], "..") == 0) {
            dirPath = realpath("..", NULL);
            if (dirPath == NULL) {
                //perror("realpath() error");
                return;
            }
            dirPath_allocated = 1;
        }
        else if (args[i][0] != '-') {
            dirPath = args[i];
            dirPath_allocated = 0;
        }
    }

    if (dirPath == NULL){
        dirPath = getcwd(NULL, 0);
        if (dirPath == NULL) {
            //perror("getcwd() failed for CWD");
            return;
        }
        dirPath_allocated = 1;
        //printf("DEBUG: dirPath set to CWD: %s\n", dirPath);  // Add this for debugging
    }

    struct stat st;
    if (stat(dirPath, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("No such directory!\n");
        if (dirPath_allocated) free(dirPath);
        return;
    }

    DIR* dir = opendir(dirPath);
    struct dirent* entry;
    if (dir == NULL) {
        printf("No such directory!\n");
        if (dirPath_allocated) free(dirPath);
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
        if (dirPath_allocated) free(dirPath);
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
                if (dirPath_allocated) free(dirPath);
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
    if (dirPath_allocated) free(dirPath);
}

void executeLog(struct atomic* atomicCmd){
    struct terminal* terminalCmd = atomicCmd->terminalArr[0];
    int argCount = terminalCmd->cmdAndArgsIndex;
    char** args = terminalCmd->cmdAndArgs;

    if (argCount == 1) {
        // No arguments: print the log
        struct executedShellCommand* current = listHead;
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
        char* endptr = NULL;
        long index = strtol(args[2], &endptr, 10);
        if (endptr == args[2] || *endptr != '\0' || index < 1 || index > logListSize) {
            fprintf(stderr, "log: Invalid Syntax!\n");
            return;
        }
        // Find the command: index 1 is newest (tail), index logListSize is oldest (head)
        int target = logListSize - (int)index + 1; // 1-based from head
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
        fprintf(stderr, "log: Invalid Syntax!\n");
    }
}

int logListSize = 0;
struct executedShellCommand* listHead = NULL;
struct executedShellCommand* listTail = NULL;


// Function to implement persistence feature for storing 15 most recent shell commands across sessions
void loadLogs(){
    const char* path = getLogFilePath();
    if (path == NULL) return;

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        return;
    }

    char buffer[1024];
    // Read one line in the file (one command) at a time and process them
    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove newline character if present
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }

        // Store the command locally in struct (string, next)
        struct executedShellCommand* newLog = malloc(sizeof(struct executedShellCommand));
        newLog->shellCommandString = strdup(buffer); // Make copy in memory (buffer will die after exiting this func)
        newLog->next = NULL;

        // listHead is the HEAD of the LL storing last 15 most recent commands across sessions
        if (listHead == NULL) {
            // If the list is empty (i.e just began loading from file), the new log is both the head and tail
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
    const char* path = getLogFilePath();
    if (path == NULL) return;

    FILE* file = fopen(path, "w");
    if (file == NULL) return;

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