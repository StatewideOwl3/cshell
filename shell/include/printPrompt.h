#ifndef PRINTPROMPT_H
#define PRINTPROMPT_H

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>

#include <unistd.h>


bool isSubstring(char* absPath, char* currPath);

char* getPathToPrint(char* absPath, char* currPath);

#endif