#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <regex.h>

struct separator{
    char sep;
    struct cmd_group* nextCmdGroup;
    struct atomic* nextAtomic;
    struct terminal* nextTerminal;
};

struct shell_cmd{
    // tokenize shell_cmd into LL of cmd_groups
    // cmd_group -> separator -> cmd_group -> separator -> .... -> (cmd_group | &) 
    // token is cmd_group, separators are & ;
    bool validity;
    struct cmd_group* cmdHead;
};


struct cmd_group{
    // linked list all atomics with separators also included?
    // token -> separator -> token -> separator -> ...-> token
    // token is atomic, separator is |
    bool validity;
    struct atomic* atomicHead;
    struct separator* separator;
};

struct atomic{
    // LL of name, input, output with separators
    // token -> token -> token -> seaparator -> token -> separator -> ... -> token
    // no necessary to have a separator b/w two tokens here!
    // token is essentially name always, separator could be ' ', <, >, >>
    bool validity;
    struct terminal* terminalHead;
    struct separator* separator;
};

struct terminal{
    bool validity;
    struct separator* separator;
};

/*
PURPOSE : checks if the shell command is made up of a bunch of command groups separated by ; or &.
INPUT : the entire string given by the user
OUTPUT : true if the string is a valid shell command, false otherwise

WORKING:  
    will tokenize shell_cmd into cmd_group using ; and & as delimiters.
    will check if each cmd_group is valid using checkCmdGroup function.
    if all cmd_groups are valid, return true, else false.
*/
bool checkShellCmd(char* shellCommand);



/*
PURPOSE : has to check if a given cmd_group string is valid
INPUT : a cmd_group string
OUTPUT : true if the cmd_group is valid, false otherwise

WORKING:
    tokenizes the cmd_group string into atomic commands delimited by | only
    then checks if each atomic command is valid using checkAtomic function
    if all atomic commands are valid, return true, else false
*/
bool checkCmdGroup(char* cmdGroup);



/*
PURPOSE : has to check if a given atomic string is valid
INPUT : an atomic command string
OUTPUT : true if the atomic command is valid, false otherwise

WORKING:
    tokenizes the atomic command into name, input and output (terminals) using < > >> as delimiters
    checks if terminal is valid using checkTerminals function
    return true if all terminal tokens are valid, else false
*/
bool checkAtomic(char* atomic);



/*
PURPOSE : has to check if a given terminal string is valid
INPUT : a terminal string
OUTPUT : true if the terminal is valid, false otherwise

WORKING:
    terminal string is valid if it does not have the letters r"[^&|;<>]"
    ie can have all chars except {&, |, ;, <, >}
    return true if valid, false if invalid
*/
bool checkTerminals(char* terminal);