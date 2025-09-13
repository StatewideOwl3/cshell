#include "../include/printPrompt.h"

bool isSubstring(char* absPath, char* currPath){
    // Check lengths. If absPath is longer, currPath cannot be a substring
    size_t absLen = strlen(absPath);
    size_t currLen = strlen(currPath);
    if (absLen > currLen) {
        return false;
    }

    // Compare absLen characters. If they match, absPath is a substring of currPath
    if (strncmp(absPath, currPath, absLen) == 0) {
        return true;
    }
    return false;
}

char* getPathToPrint(char* absPath, char* currPath){
    if (isSubstring(absPath, currPath)){
        size_t remainingLen = strlen(currPath) - strlen(absPath);
        char* newPath = (char*)malloc(remainingLen + 2); // +1 for '~', +1 for null terminator
        if (newPath == NULL) {
            perror("malloc failed");
            return NULL;
        }
        newPath[0] = '~';
        strcpy(newPath + 1, currPath + strlen(absPath));
        return newPath;
    } else {
        char* newPath = (char*)malloc(strlen(currPath) + 1);
        if (newPath == NULL) {
            perror("malloc failed");
            return NULL;
        }
        strcpy(newPath, currPath);
        return newPath;
    }
}

// LLM ENDS