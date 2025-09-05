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

void executeHop(struct shell_cmd* shellCommandStruct);

void executeReveal(struct shell_cmd* shellCommandStruct);