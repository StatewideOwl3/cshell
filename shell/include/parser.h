#ifndef PARSER_H
#define PARSER_H


#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <regex.h>


struct shell_cmd{

    bool validity;
    struct cmd_group** cmdGroupArr; // array of pointers to cmd_group tokens
    char** separatorArr;
    int cmdArrIndex;
    int sepArrIndex;
};


struct cmd_group{
    char* cmdString;
    bool validity;
    struct atomic** atomicArr;
    char** separatorArr; 
    int atomicArrIndex;
    int sepArrIndex;
    
};

struct atomic{

    char* atomicString;
    bool validity;
    struct terminal** terminalArr; // array of name types
    char** separatorArr;
    int termArrIndex;
    int sepArrIndex;
};

struct terminal{
    char* terminalString; // string of the terminal to tokenize further
    char** cmdAndArgs; // array of strings
    int cmdAndArgsIndex;
    bool validity;
};



/*
    tokenize the shell command into cmd_groups and separators
    build an array of cmd_group struct pointers filled in with details and separators 

*/

struct shell_cmd* tokenizeShellCommand(char* shellCommandString);

bool checkShellCmd(struct shell_cmd* shellCommand);


/*
    for each cmd_group in cmdGroupArr, from the string field, build the atomic and 
    separator arrays.
*/

struct cmd_group* tokenizeCmdGroup(struct cmd_group* cmdGroup);

bool checkCmdGroup(struct cmd_group* cmdGroup);


/*
    for each atomic in atomicArr, take the string and build the terminal and
    separator arrays.
*/

struct atomic* tokenizeAtomic(struct atomic* atomicGroup);

bool checkAtomic(struct atomic* atomicGroup);



struct terminal* tokenizeTerminal(struct terminal* terminalGroup);

bool checkTerminals(char* terminal);



struct shell_cmd* verifyCommand(char* inputCommand);


void testAllTokenizers();
void testTokenizeShellCommand();
void testTokenizeCmdGroup();
void testTokenizeAtomic();
void testTokenizeShellAndCmdGroup();

void printIndent(int n);
bool isWhitespace(char c);



void freeAtomic(struct atomic* atomicGroup);
void freeCmdGroup(struct cmd_group* cmdGroup);
void freeShellCmd(struct shell_cmd* shellCommand);

#endif // PARSER_H