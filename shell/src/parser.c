#include "../include/parser.h"

/*
    tokenize the shell command into cmd_groups and separators
    build an array of cmd_group struct pointers filled in with details and separators 

*/

// helper function to check if a character is whitespace
bool isWhitespace(char c) {
    regex_t regex;
    int reti;
    char str[2] = {c, '\0'};

    // Compile regex for whitespace
    reti = regcomp(&regex, "[[:space:]]", REG_NOSUB | REG_EXTENDED);
    if (reti) return false;

    // Execute regex
    reti = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);

    return reti == 0;
}

struct shell_cmd* tokenizeShellCommand(char* shellCommandString){
    ////////////////////////////// LLM GENERATED CODE BEGNS /////////////////////////////////
    // Allocate memory for shell_cmd struct
    struct shell_cmd* shellCommand = (struct shell_cmd*)malloc(sizeof(struct shell_cmd));
    if (shellCommand == NULL) {
        perror("Failed to allocate memory for shell_cmd");
        return NULL;
    }

    // Initialize indices and validity
    shellCommand->cmdArrIndex = 0;
    shellCommand->sepArrIndex = 0;
    shellCommand->validity = false;

    // Allocate initial memory for cmdGroupArr and separatorArr
    int initialSize = 10; // Initial size, can be adjusted
    shellCommand->cmdGroupArr = (struct cmd_group**)malloc(initialSize * sizeof(struct cmd_group*));
    shellCommand->separatorArr = (char**)malloc(initialSize * sizeof(char*));
    if (shellCommand->cmdGroupArr == NULL || shellCommand->separatorArr == NULL) {
        perror("Failed to allocate memory for cmdGroupArr or separatorArr");
        free(shellCommand);
        return NULL;
    }
    ////////////////////////////// LLM GENERATED CODE ENDS /////////////////////////////////
    

    int stringLength = strlen(shellCommandString);
    int i = 0;
    while(i<stringLength){
        if (isWhitespace(shellCommandString[i])){
            i++;
            continue;
        }

        // when we encounter a non-whitespace character, it indicates the start of a command
        int cmdInstanceStart = i;
        struct cmd_group* cmdGroupInstance = (struct cmd_group*)malloc(sizeof(struct cmd_group));
        if (cmdGroupInstance == NULL) {
            perror("Failed to allocate memory for cmd_group");
            // functions to free memory at each level
            return NULL;
        }
        
        while(i<stringLength && (shellCommandString[i]!=';' && shellCommandString[i]!='&' )){
            i++;
        }

        int cmdInstanceEnd = i;
        int cmdInstanceLength = cmdInstanceEnd - cmdInstanceStart;
        cmdGroupInstance->cmdString = (char*)malloc((cmdInstanceLength + 1) * sizeof(char));
        if (cmdGroupInstance->cmdString == NULL) {
            perror("Failed to allocate memory for cmdString");
            // functions to free memory at each level
            return NULL;
        }

        // copy the substring into the array
        for (int temp = cmdInstanceStart; temp < cmdInstanceEnd; temp++) {
            cmdGroupInstance->cmdString[temp - cmdInstanceStart] = shellCommandString[temp];
        }
        cmdGroupInstance->cmdString[cmdInstanceLength] = '\0'; // null-terminate the string
        
        char* separatorChar = (char*)malloc(2 * sizeof(char));
        if (separatorChar == NULL) {
            perror("Failed to allocate memory for separatorChar");
            // functions to free memory at each level
            return NULL;
        }

        separatorChar[0] = shellCommandString[i];
        separatorChar[1] = '\0';

        // Add cmdGroupInstance and separatorChar to shellCommand's arrays
        if (shellCommand->cmdArrIndex >= initialSize) {
            // Reallocate memory if needed
            initialSize *= 2;
            shellCommand->cmdGroupArr = (struct cmd_group**)realloc(shellCommand->cmdGroupArr, initialSize * sizeof(struct cmd_group*));
            shellCommand->separatorArr = (char**)realloc(shellCommand->separatorArr, initialSize * sizeof(char*));
            if (shellCommand->cmdGroupArr == NULL || shellCommand->separatorArr == NULL) {
                perror("Failed to reallocate memory for cmdGroupArr or separatorArr");
                // functions to free memory at each level
            }
        }
        shellCommand->cmdGroupArr[shellCommand->cmdArrIndex++] = cmdGroupInstance;
        shellCommand->separatorArr[shellCommand->sepArrIndex++] = separatorChar;

        // Move to the next character after the separator
        i++;
    }

    return shellCommand;
}

bool checkShellCmd(struct shell_cmd* shellCommand){
    if (!shellCommand) return false;
    for (int i = 0; i < shellCommand->cmdArrIndex; i++) {
        if (!checkCmdGroup(shellCommand->cmdGroupArr[i])) return false;
    }
    // Optionally: check for valid use of & and ; separators
    shellCommand->validity = true;
    return true;
}


/*
    for each cmd_group in cmdGroupArr, from the string field, build the atomic and 
    separator arrays.
*/

struct cmd_group* tokenizeCmdGroup(char* cmdString){
    ////////////////////////////// LLM GENERATED CODE BEGNS /////////////////////////////////
    // Allocate memory for cmd_group struct
    struct cmd_group* cmdGroup = (struct cmd_group*)malloc(sizeof(struct cmd_group));
    if (cmdGroup == NULL) {
        perror("Failed to allocate memory for cmd_group");
        // functions to free memory at each level
        return NULL;
    }

    // Initialize indices and validity
    cmdGroup->atomicArrIndex = 0;
    cmdGroup->sepArrIndex = 0;
    cmdGroup->validity = false;

    // Allocate initial memory for atomicArr and separatorArr
    int initialSize = 10; // Initial size, can be adjusted
    cmdGroup->atomicArr = (struct atomic**)malloc(initialSize * sizeof(struct atomic*));
    cmdGroup->separatorArr = (char**)malloc(initialSize * sizeof(char*));
    if (cmdGroup->atomicArr == NULL || cmdGroup->separatorArr == NULL) {
        perror("Failed to allocate memory for atomicArr or separatorArr");
        // functions to free memory properly
        return NULL;
    }
    

    int stringLength = strlen(cmdString);
    int i = 0;
    while(i<stringLength){
        if (isWhitespace(cmdString[i])){
            i++;
            continue;
        }

        // when we encounter a non-whitespace character, it indicates the start of an atomic command
        int atomicInstanceStart = i;
        struct atomic* atomicInstance = (struct atomic*)malloc(sizeof(struct atomic));
        if (atomicInstance == NULL) {
            perror("Failed to allocate memory for atomic");
            // functions to free memory at each level
            return NULL;
        }
        
        while(i<stringLength && cmdString[i]!='|'){
            i++;
        }

        int atomicInstanceEnd = i;
        int atomicInstanceLength = atomicInstanceEnd - atomicInstanceStart;
        atomicInstance->atomicString = (char*)malloc((atomicInstanceLength + 1) * sizeof(char));
        if (atomicInstance->atomicString == NULL) {
            perror("Failed to allocate memory for atomicString");
            // functions to free memory at each level
            return NULL;
        }

        // copy the substring into the array
        for (int temp = atomicInstanceStart; temp < atomicInstanceEnd; temp++) {
            atomicInstance->atomicString[temp - atomicInstanceStart] = cmdString[temp];
        }
        atomicInstance->atomicString[atomicInstanceLength] = '\0'; // null-terminate the string

        char* separatorChar = (char*)malloc(2 * sizeof(char));
        if (separatorChar == NULL) {
            perror("Failed to allocate memory for separatorChar");
            // functions to free memory at each level
            return NULL;
        }
        separatorChar[0] = cmdString[i];
        separatorChar[1] = '\0';

        // Add atomicInstance and separatorChar to cmdGroup's arrays
        if (cmdGroup->atomicArrIndex >= initialSize) {
            // Reallocate memory if needed
            initialSize *= 2;
            cmdGroup->atomicArr = (struct atomic**)realloc(cmdGroup->atomicArr, initialSize * sizeof(struct atomic*));
            cmdGroup->separatorArr = (char**)realloc(cmdGroup->separatorArr, initialSize * sizeof(char*));
            if (cmdGroup->atomicArr == NULL || cmdGroup->separatorArr == NULL) {
                perror("Failed to reallocate memory for atomicArr or separatorArr");
                // functions to free memory at each level
                return NULL;
            }
        }
        cmdGroup->atomicArr[cmdGroup->atomicArrIndex++] = atomicInstance;
        cmdGroup->separatorArr[cmdGroup->sepArrIndex++] = separatorChar;

        // Move to the next character after the separator
        i++;
    }
    return cmdGroup;
}

bool checkCmdGroup(struct cmd_group* cmdGroup){
    if (!cmdGroup) return false;
    for (int i = 0; i < cmdGroup->atomicArrIndex; i++) {
        if (!checkAtomic(cmdGroup->atomicArr[i])) return false;
    }
    // Optionally: check for valid pipe usage (no consecutive pipes, etc.)
    cmdGroup->validity = true;
    return true;
}


/*
    for each atomic in atomicArr, take the string and build the terminal and
    separator arrays.
*/
struct atomic* tokenizeAtomic(char* atomicString){
    ////////////////////////////// LLM GENERATED CODE BEGNS /////////////////////////////////
    // Allocate memory for atomic struct
    struct atomic* atomicGroup = (struct atomic*)malloc(sizeof(struct atomic));
    if (atomicGroup == NULL) {
        perror("Failed to allocate memory for atomic");
        // functions to free memory at each level
        return NULL;
    }

    // Initialize indices and validity
    atomicGroup->termArrIndex = 0;
    atomicGroup->sepArrIndex = 0;
    atomicGroup->validity = false;

    // Allocate initial memory for terminalArr and separatorArr
    int initialSize = 10; // Initial size, can be adjusted
    atomicGroup->terminalArr = (char**)malloc(initialSize * sizeof(char*));
    atomicGroup->separatorArr = (char**)malloc(initialSize * sizeof(char*));
    if (atomicGroup->terminalArr == NULL || atomicGroup->separatorArr == NULL) {
        perror("Failed to allocate memory for terminalArr or separatorArr");
        // functions to free memory properly
        return NULL;
    }
    

    int stringLength = strlen(atomicString);
    int i = 0;
    while(i<stringLength){
        if (isWhitespace(atomicString[i])){
            i++;
            continue;
        }

        // when we encounter a non-whitespace character, it indicates the start of a terminal
        int termInstanceStart = i;
        
        // a terminal ends when we hit whitespace or a redirection operator
        while(i<stringLength && !(atomicString[i]=='<' || atomicString[i]=='>' )){
            i++;
        }

        int termInstanceEnd = i;
        int termInstanceLength = termInstanceEnd - termInstanceStart;
        char* terminalInstance = (char*)malloc((termInstanceLength + 1) * sizeof(char));
        if (terminalInstance == NULL) {
            perror("Failed to allocate memory for terminalInstance");
            // functions to free memory at each level
            return NULL;
        }
        strncpy(terminalInstance, &atomicString[termInstanceStart], termInstanceLength);
        terminalInstance[termInstanceLength] = '\0'; // null-terminate the string


        char* separatorChar = (char*)malloc(2 * sizeof(char));
        if (separatorChar == NULL) {
            perror("Failed to allocate memory for separatorChar");
            // functions to free memory at each level
            return NULL;
        }

        if (atomicString[i]=='>' && atomicString[i+1]=='>'){
            separatorChar = (char*)realloc(separatorChar, 3 * sizeof(char));
            separatorChar[0] = '>';
            separatorChar[1] = '>';
            separatorChar[2] = '\0';
            i+=2;
        }
        else if (atomicString[i]=='>' || atomicString[i]=='<'){
            separatorChar[0] = atomicString[i];
            separatorChar[1] = '\0';
            i++;
        }
        else{
            separatorChar[0] = '\0'; // No separator
        }
        
        // Add terminalInstance and separatorChar to atomicGroup's arrays
        if (atomicGroup->termArrIndex >= initialSize) {
            // Reallocate memory if needed
            initialSize *= 2;
            atomicGroup->terminalArr = (char**)realloc(atomicGroup->terminalArr, initialSize * sizeof(char*));
            atomicGroup->separatorArr = (char**)realloc(atomicGroup->separatorArr, initialSize * sizeof(char*));
            if (atomicGroup->terminalArr == NULL || atomicGroup->separatorArr == NULL) {
                perror("Failed to reallocate memory for terminalArr or separatorArr");
                // functions to free memory at each level
                return NULL;
            }
        }

        atomicGroup->terminalArr[atomicGroup->termArrIndex++] = terminalInstance;
        atomicGroup->separatorArr[atomicGroup->sepArrIndex++] = separatorChar;
        
        // Move to the next character after the separator
        i++;
    }
    return atomicGroup;
    ////////////////////////////// LLM GENERATED CODE ENDS /////////////////////////////////
}

bool checkAtomic(struct atomic* atomicGroup){
    if (atomicGroup->termArrIndex == 0) {
        return false; // No terminals found
    }    

    // Check each terminal for validity 
    for (int i = 0; i < atomicGroup->termArrIndex; i++) {
        if (!checkTerminals(atomicGroup->terminalArr[i])) {
            return false;
        }
    }

    if (strcmp(atomicGroup->separatorArr[atomicGroup->sepArrIndex-1], "|") == 0){
        return false; // Last separator should be empty
    }
    atomicGroup->validity = true;
    return true;
}



bool checkTerminals(char* terminal){
    // should not be empty or just whitespace
    if (terminal == NULL || strlen(terminal) == 0) return false;

    // should be in the scanset of valid characters r"[^|;&<>]+"
    int length = strlen(terminal);
    for (int i = 0; i < length; i++) {
        char c = terminal[i];
        if (c == '|' || c == ';' || c == '&' || c == '<' || c == '>') {
            return false;
        }
    }
    return true;
}


void testTokenizeShellCommand() {
    char* testCommand = "ls -l; echo Hello & pwd";
    struct shell_cmd* result = tokenizeShellCommand(testCommand);

    if (result == NULL) {
        printf("Tokenization failed.\n");
        return;
    }

    printf("Number of command groups: %d\n", result->cmdArrIndex);
    for (int i = 0; i < result->cmdArrIndex; i++) {
        printf("Command Group %d: %s\n", i + 1, result->cmdGroupArr[i]->cmdString);
        if (i < result->sepArrIndex) {
            printf("Separator %d: %s\n", i + 1, result->separatorArr[i]);
        }
    }

    // Free allocated memory (not shown here for brevity)
}

void testTokenizeCmdGroup() {
    char* testCmdGroup = "ls -l  >> name.txt | grep txt < name.txt | wc -l";
    struct cmd_group* result = tokenizeCmdGroup(testCmdGroup);

    if (result == NULL) {
        printf("Tokenization failed.\n");
        return;
    }

    printf("Number of atomic commands: %d\n", result->atomicArrIndex);
    for (int i = 0; i < result->atomicArrIndex; i++) {
        printf("Atomic Command %d: %s\n", i + 1, result->atomicArr[i]->atomicString);
        if (i < result->sepArrIndex) {
            printf("Separator %d: %s\n", i + 1, result->separatorArr[i]);
        }
    }

    // Free allocated memory (not shown here for brevity)
}

void testTokenizeShellAndCmdGroup() {
    char* testCommand = "ls -l; echo Hello & pwd; ls -l  >> name.txt | grep txt < name.txt | wc -l &";
    struct shell_cmd* shellResult = tokenizeShellCommand(testCommand);

    if (shellResult == NULL) {
        printf("Shell command tokenization failed.\n");
        return;
    }

    printf("Number of command groups: %d\n", shellResult->cmdArrIndex);
    for (int i = 0; i < shellResult->cmdArrIndex; i++) {
        struct cmd_group* cmdGroup = shellResult->cmdGroupArr[i];
        printf("Command Group %d: %s\n", i + 1, cmdGroup->cmdString);

        struct cmd_group* cmdGroupResult = tokenizeCmdGroup(cmdGroup->cmdString);
        if (cmdGroupResult == NULL) {
            printf("Cmd group tokenization failed for command group %d.\n", i + 1);
            continue;
        }

        printf("  Number of atomic commands: %d\n", cmdGroupResult->atomicArrIndex);
        for (int j = 0; j < cmdGroupResult->atomicArrIndex; j++) {
            printf("  Atomic Command %d: %s\n", j + 1, cmdGroupResult->atomicArr[j]->atomicString);
            if (j < cmdGroupResult->sepArrIndex) {
                printf("  Separator %d: %s\n", j + 1, cmdGroupResult->separatorArr[j]);
            }
        }

        // Free allocated memory for cmdGroupResult (not shown here for brevity)
    }

    // Free allocated memory for shellResult (not shown here for brevity)
}

void testTokenizeAtomic() {
    char* testAtomic = "ls -l  >> name.txt";
    struct atomic* result = tokenizeAtomic(testAtomic);

    if (result == NULL) {
        printf("Tokenization failed.\n");
        return;
    }

    printf("Number of terminals: %d\n", result->termArrIndex);
    for (int i = 0; i < result->termArrIndex; i++) {
        printf("Terminal %d: %s\n", i + 1, result->terminalArr[i]);
        if (i < result->sepArrIndex) {
            printf("Separator %d: %s\n", i + 1, result->separatorArr[i]);
        }
    }

    // Free allocated memory (not shown here for brevity)
}

void printIndent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

void testAllTokenizers(){
    char* shell_cmd = "ls -l ;; echo Hello & & pwd";
    printf("{\n");
    printf("  \"shell_cmd\": \"%s\",\n", shell_cmd);

    struct shell_cmd* shellResult = tokenizeShellCommand(shell_cmd);

    if (shellResult == NULL) {
        printf("  \"error\": \"Shell command tokenization failed.\"\n}\n");
        return;
    }

    printf("  \"command_groups\": [\n");
    for (int i = 0; i < shellResult->cmdArrIndex; i++) {
        printIndent(2);
        printf("{\n");
        printIndent(3);
        printf("\"cmd_group\": \"%s\",\n", shellResult->cmdGroupArr[i]->cmdString);
        if (i < shellResult->sepArrIndex) {
            printIndent(3);
            printf("\"separator\": \"%s\",\n", shellResult->separatorArr[i]);
        }

        struct cmd_group* cmdGroupResult = tokenizeCmdGroup(shellResult->cmdGroupArr[i]->cmdString);
        if (cmdGroupResult == NULL) {
            printIndent(3);
            printf("\"error\": \"Cmd group tokenization failed\"\n");
            printIndent(2);
            printf("}%s\n", (i == shellResult->cmdArrIndex - 1) ? "" : ",");
            continue;
        }

        printIndent(3);
        printf("\"atomic_commands\": [\n");
        for (int j = 0; j < cmdGroupResult->atomicArrIndex; j++) {
            printIndent(4);
            printf("{\n");
            printIndent(5);
            printf("\"atomic\": \"%s\",\n", cmdGroupResult->atomicArr[j]->atomicString);
            if (j < cmdGroupResult->sepArrIndex) {
                printIndent(5);
                printf("\"separator\": \"%s\",\n", cmdGroupResult->separatorArr[j]);
            }

            struct atomic* atomicResult = tokenizeAtomic(cmdGroupResult->atomicArr[j]->atomicString);
            if (atomicResult == NULL) {
                printIndent(5);
                printf("\"error\": \"Atomic tokenization failed\"\n");
                printIndent(4);
                printf("}%s\n", (j == cmdGroupResult->atomicArrIndex - 1) ? "" : ",");
                continue;
            }

            printIndent(5);
            printf("\"terminals\": [\n");
            for (int k = 0; k < atomicResult->termArrIndex; k++) {
                printIndent(6);
                printf("{ \"terminal\": \"%s\"", atomicResult->terminalArr[k]);
                if (k < atomicResult->sepArrIndex && atomicResult->separatorArr[k][0] != '\0') {
                    printf(", \"separator\": \"%s\"", atomicResult->separatorArr[k]);
                }
                printf(" }%s\n", (k == atomicResult->termArrIndex - 1) ? "" : ",");
            }
            printIndent(5);
            printf("]\n");
            printIndent(4);
            printf("}%s\n", (j == cmdGroupResult->atomicArrIndex - 1) ? "" : ",");
        }
        printIndent(3);
        printf("]\n");
        printIndent(2);
        printf("}%s\n", (i == shellResult->cmdArrIndex - 1) ? "" : ",");
    }
    printf("  ]\n");
    printf("}\n");
}

void verifyCommand(char* inputCommand){
    char* shell_cmd = inputCommand; // You can change this to test other commands
    printf("Input Command: %s\n", shell_cmd);
    struct shell_cmd* shellResult = tokenizeShellCommand(shell_cmd);
    if (shellResult == NULL) {
        printf("Shell command tokenization failed.\n");
        return;
    }

    for (int i = 0; i < shellResult->cmdArrIndex; i++) {
        struct cmd_group* cmdGroupResult = tokenizeCmdGroup(shellResult->cmdGroupArr[i]->cmdString);
        if (cmdGroupResult == NULL) {
            printf("Cmd group tokenization failed for command group %d.\n", i + 1);
            return;
        }
        shellResult->cmdGroupArr[i] = cmdGroupResult;
        for (int j = 0; j < cmdGroupResult->atomicArrIndex; j++) {
            struct atomic* atomicResult = tokenizeAtomic(cmdGroupResult->atomicArr[j]->atomicString);
            if (atomicResult == NULL) {
                printf("Atomic tokenization failed for atomic command %d in command group %d.\n", j + 1, i + 1);
                return;
            }
            cmdGroupResult->atomicArr[j] = atomicResult;
        }
    }



    bool valid = checkShellCmd(shellResult);

    if (valid) {
        printf("Valid Syntax!\n");
    } else {
        printf("Invalid Syntax!\n");
    }

    // Free allocated memory for shellResult and its children here
}

// int main() {
//     char* inputCommand = "ls -l | | grep txt";
//     verifyCommand(inputCommand);
//     return 0;
// }