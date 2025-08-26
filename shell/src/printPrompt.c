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
    //printf("enetered get path to print\n");
    // Check if absPath is a substring of currPath
    
    if (isSubstring(absPath, currPath)){
        // Return the substring of currPath that comes after absPath
        //printf("absPath is a substring of currPath\n");
        //printf("absPath: %s, currPath: %s\n", absPath, currPath);
        char* newPath = (char*)malloc(sizeof(char)*(strlen(currPath) - strlen(absPath) + 2)); // +2 for '~' and null terminator
        newPath[0]='~';
        strncpy(newPath + 1, currPath + strlen(absPath), strlen(newPath)-1);
        //printf("newPath: %s\n", newPath);
        return newPath;
    } else {
        // If absPath is not a substring, return currPath as is
        char* newPath = (char*)malloc(sizeof(char)*(strlen(currPath)+1));
        strcpy(newPath, currPath);
        return newPath;
    }
}