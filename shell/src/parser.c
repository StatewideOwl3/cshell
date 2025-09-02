#include "../include/parser.h"


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
bool checkAtomic(char* atomic){
    // of the form "name (name | input | output)*" have to tokenize this
    // === "name (name | < name | > name | >> name)"

    int total_len = strlen(atomic);
    if (total_len == 0){
        return false; // empty atomic command is invalid
    }

    for (int i = 0; i<total_len;i++){
        if (i== ' ' || i== '\t' || i== '\n' || i== '\r')
            continue;

        if (atomic[i]!='&' && atomic[i]!='|' && atomic[i]!=';'){
            int first_char_index = i;
            while(atomic[i]!='&' && atomic[i]!='|' && atomic[i]!=';'){
                i++;
            }
            char* terminal = (char*)malloc(sizeof(char)*(i-first_char_index+1));
            strncpy(terminal, atomic+first_char_index, i-first_char_index);
            if (!checkTerminals(terminal)){
                false;
            }
        }
    }

}



/*
PURPOSE : has to check if a given terminal string is valid
INPUT : a terminal string
OUTPUT : true if the terminal is valid, false otherwise

WORKING:
    terminal string is valid if it does not have the letters r"[^&|;<>]"
    ie can have all chars except {&, |, ;, <, >}
    return true if valid, false if invalid
*/
bool checkTerminals(char* terminal){
    // assuming checkAtomic has tokenized atomic string into name input and output only.
    int len = strlen(terminal);
    for (int i=0;i<len;i++){
        if (terminal[i] == '&' || terminal[i] == '|' || terminal[i] == ';' || terminal[i] == '<' || terminal[i] == '>'){
            return false;
        }
    }
    return true;
}