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
    char** terminalArr; // array of name types
    char** separatorArr;
    int termArrIndex;
    int sepArrIndex;
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

struct cmd_group* tokenizeCmdGroup(char* cmdString);

bool checkCmdGroup(struct cmd_group* cmdGroup);


/*
    for each atomic in atomicArr, take the string and build the terminal and
    separator arrays.
*/

struct atomic* tokenizeAtomic(char* atomicString);

bool checkAtomic(struct atomic* atomicGroup);



bool checkTerminals(char* terminal);




void verifyCommand(char* inputCommand);
void testAllTokenizers();
void testTokenizeShellCommand();
void testTokenizeCmdGroup();
void testTokenizeAtomic();
void testTokenizeShellAndCmdGroup();

void printIndent(int n);
bool isWhitespace(char c);
