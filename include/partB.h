#ifndef PARTB_H
#define PARTB_H

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
 #include <sys/stat.h>

#include "printPrompt.h"
#include "parser.h"
#include "executes.h"

extern char* absoluteHomePath; // Global variable to hold the absolute home path

extern char* oldWD;

#define MAX_HISTORY 15

struct executedShellCommand{
    char* shellCommandString;
    struct executedShellCommand* next;
};

void executeHop(struct atomic* atomicCmd);

void executeReveal(struct atomic* atomicCmd);

bool checkRevealSyntax(struct atomic* atomicGroup);

void executeLog(struct atomic* atomicCmd);


extern char* logFile;
extern int logListSize;
extern struct executedShellCommand* listHead;
extern struct executedShellCommand* listTail;


void loadLogs();

void saveLog();

void addLog(char* commandString);

#endif // PARTB_H
