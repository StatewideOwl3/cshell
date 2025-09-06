#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include "printPrompt.h"
#include "parser.h"

extern char* absoluteHomePath; // Global variable to hold the absolute home path

extern char* oldWD;

void executeHop(struct atomic* atomicCmd);

void executeReveal(struct atomic* atomicCmd);

bool checkRevealSyntax(struct atomic* atomicGroup);

void executeLog(struct atomic* atomicCmd);