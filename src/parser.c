#include "../include/parser.h"


void freeTerminal(struct terminal* terminalGroup){
    if (terminalGroup == NULL) return;

    // Free terminalString
    if (terminalGroup->terminalString != NULL) {
        free(terminalGroup->terminalString);
    }

    // Free cmdAndArgs
    if (terminalGroup->cmdAndArgs != NULL) {
        for (int i = 0; i < terminalGroup->cmdAndArgsIndex; i++) {
            if (terminalGroup->cmdAndArgs[i] != NULL) {
                free(terminalGroup->cmdAndArgs[i]);
            }
        }
        free(terminalGroup->cmdAndArgs);
    }
    // Finally, free the terminalGroup struct itself
    free(terminalGroup);
}

void freeAtomic(struct atomic* atomicGroup){
    if (atomicGroup == NULL) return;

    // Free atomicString
    if (atomicGroup->atomicString != NULL) {
        free(atomicGroup->atomicString);
    }

    // Free terminalArr
    if (atomicGroup->terminalArr != NULL) {
        for (int i = 0; i < atomicGroup->termArrIndex; i++) {
            if (atomicGroup->terminalArr[i] != NULL) {
                freeTerminal(atomicGroup->terminalArr[i]);
            }
        }
        free(atomicGroup->terminalArr);
    }

    // Free separatorArr
    if (atomicGroup->separatorArr != NULL) {
        for (int i = 0; i < atomicGroup->sepArrIndex; i++) {
            if (atomicGroup->separatorArr[i] != NULL) {
                free(atomicGroup->separatorArr[i]);
            }
        }
        free(atomicGroup->separatorArr);
    }
    // Finally, free the atomicGroup struct itself
    free(atomicGroup);
}

void freeCmdGroup(struct cmd_group* cmdGroup){
    if (cmdGroup == NULL) return;

    // Free cmdString
    if (cmdGroup->cmdString != NULL) {
        free(cmdGroup->cmdString);
    }

    // Free atomicArr
    if (cmdGroup->atomicArr != NULL) {
        for (int i = 0; i < cmdGroup->atomicArrIndex; i++) {
            if (cmdGroup->atomicArr[i] != NULL) {
                freeAtomic(cmdGroup->atomicArr[i]);
            }
        }
        free(cmdGroup->atomicArr);
    }

    // Free separatorArr
    if (cmdGroup->separatorArr != NULL) {
        for (int i = 0; i < cmdGroup->sepArrIndex; i++) {
            if (cmdGroup->separatorArr[i] != NULL) {
                free(cmdGroup->separatorArr[i]);
            }
        }
        free(cmdGroup->separatorArr);
    }
    // Finally, free the cmdGroup struct itself
    free(cmdGroup);
}

void freeShellCmd(struct shell_cmd* shellCommand){
    if (shellCommand == NULL) return;

    // Free cmdGroupArr
    if (shellCommand->cmdGroupArr != NULL) {
        for (int i = 0; i < shellCommand->cmdArrIndex; i++) {
            if (shellCommand->cmdGroupArr[i] != NULL) {
                freeCmdGroup(shellCommand->cmdGroupArr[i]);
            }
        }
        free(shellCommand->cmdGroupArr);
    }

    // Free separatorArr
    if (shellCommand->separatorArr != NULL) {
        for (int i = 0; i < shellCommand->sepArrIndex; i++) {
            if (shellCommand->separatorArr[i] != NULL) {
                free(shellCommand->separatorArr[i]);
            }
        }
        free(shellCommand->separatorArr);
    }
    // Finally, free the shellCommand struct itself
    free(shellCommand);
}



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
        //perror("Failed to allocate memory for shell_cmd");
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
        //perror("Failed to allocate memory for cmdGroupArr or separatorArr");
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
        // Create a cmd_group struct to capture this cmd_group
        struct cmd_group* cmdGroupInstance = (struct cmd_group*)malloc(sizeof(struct cmd_group));
        if (cmdGroupInstance == NULL) {
            //perror("Failed to allocate memory for cmd_group");
            // functions to free memory at each level
            return NULL;
        }
        
        // Parse till to find end of the cmd_group instance (; or & is seen)
        while(i<stringLength && (shellCommandString[i]!=';' && shellCommandString[i]!='&' )){
            i++;
        }

        int cmdInstanceEnd = i;
        int cmdInstanceLength = cmdInstanceEnd - cmdInstanceStart;
        cmdGroupInstance->cmdString = (char*)malloc((cmdInstanceLength + 1) * sizeof(char));
        if (cmdGroupInstance->cmdString == NULL) {
            //perror("Failed to allocate memory for cmdString");
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
            //perror("Failed to allocate memory for separatorChar");
            // functions to free memory at each level
            return NULL;
        }

        separatorChar[0] = shellCommandString[i]; // stopped when ; or & was reached
        separatorChar[1] = '\0';

        // Add cmdGroupInstance and separatorChar to shellCommand's arrays
        if (shellCommand->cmdArrIndex >= initialSize) {
            // Reallocate memory if needed
            initialSize *= 2;
            shellCommand->cmdGroupArr = (struct cmd_group**)realloc(shellCommand->cmdGroupArr, initialSize * sizeof(struct cmd_group*));
            shellCommand->separatorArr = (char**)realloc(shellCommand->separatorArr, initialSize * sizeof(char*));
            if (shellCommand->cmdGroupArr == NULL || shellCommand->separatorArr == NULL) {
                //perror("Failed to reallocate memory for cmdGroupArr or separatorArr");
                // functions to free memory at each level
                return NULL;
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

    int totalCmds = shellCommand->cmdArrIndex;
    int totalSeps = shellCommand->sepArrIndex;

    // Check for empty command groups
    for (int i = 0; i < totalCmds; i++) {
        struct cmd_group* cmdGroup = shellCommand->cmdGroupArr[i];
        
        if (cmdGroup->cmdString == NULL){
            //printf("cmd string is a null pointer\n");
            return false;
        }
        
        if (strlen(shellCommand->cmdGroupArr[i]->cmdString) == 0) {
            shellCommand->validity = false;
            //printf("found an empty command group or it has an empty string\n");
            return false;
        }
    }

    // Last separator should be empty or ampersand (if present)
    if (totalSeps > 0) {
        char* lastSep = shellCommand->separatorArr[totalSeps - 1];
        if (!(strcmp(lastSep, "") == 0 || strcmp(lastSep, "&") == 0)) {
            shellCommand->validity = false;
            //printf("last separator not '' or 'ampersand'\n");
            return false;
        }
    }

    // Check each command group for validity
    for (int i = 0; i < shellCommand->cmdArrIndex; i++) {
        //printf("Checking validity of each command: %d of %d\n", i+1, shellCommand->cmdArrIndex);
        if (!checkCmdGroup(shellCommand->cmdGroupArr[i])){
            //printf("checkCmdGroup returned false for the command group '%s'\n",shellCommand->cmdGroupArr[i]->cmdString);
            shellCommand->validity = false;
            return false;
        }
    }

    shellCommand->validity = true;
    return true;
}


/*
    for each cmd_group in cmdGroupArr, from the string field, build the atomic and 
    separator arrays.
*/

struct cmd_group* tokenizeCmdGroup(struct cmd_group* cmdGroup){
    ////////////////////////////// LLM GENERATED CODE BEGNS /////////////////////////////////
    // Passed struct only has cmd_string filled in
    // Initialize indices and validity
    cmdGroup->atomicArrIndex = 0;
    cmdGroup->sepArrIndex = 0;
    cmdGroup->validity = false;

    // Allocate initial memory for atomicArr and separatorArr
    int initialSize = 10; // Initial size, can be adjusted
    cmdGroup->atomicArr = (struct atomic**)malloc(initialSize * sizeof(struct atomic*));
    cmdGroup->separatorArr = (char**)malloc(initialSize * sizeof(char*));
    if (cmdGroup->atomicArr == NULL || cmdGroup->separatorArr == NULL) {
        //perror("Failed to allocate memory for atomicArr or separatorArr");
        // functions to free memory properly
        return NULL;
    }
    
    char* cmdString = cmdGroup->cmdString;
    int stringLength = strlen(cmdString);
    
    // Parse the command string to extract atomics
    int i = 0;
    while(i<stringLength){
        if (isWhitespace(cmdString[i])){
            i++;
            continue;
        }

        // when we encounter a non-whitespace character, it indicates the start of an atomic command
        int atomicInstanceStart = i;
        // Create atomic struct (this is what cmd_group is made of)
        struct atomic* atomicInstance = (struct atomic*)malloc(sizeof(struct atomic));
        if (atomicInstance == NULL) {
            //perror("Failed to allocate memory for atomic");
            // functions to free memory at each level
            return NULL;
        }
        
        // Read an atomic group
        while(i<stringLength && cmdString[i]!='|'){
            i++;
        }

        int atomicInstanceEnd = i;
        int atomicInstanceLength = atomicInstanceEnd - atomicInstanceStart;
        atomicInstance->atomicString = (char*)malloc((atomicInstanceLength + 1) * sizeof(char));
        if (atomicInstance->atomicString == NULL) {
            //perror("Failed to allocate memory for atomicString");
            // functions to free memory at each level
            return NULL;
        }

        // Copy atomic string found in the cmd_group into atomic struct
        for (int temp = atomicInstanceStart; temp < atomicInstanceEnd; temp++) {
            atomicInstance->atomicString[temp - atomicInstanceStart] = cmdString[temp];
        }
        atomicInstance->atomicString[atomicInstanceLength] = '\0'; // null-terminate the string

        char* separatorChar = (char*)malloc(2 * sizeof(char));
        if (separatorChar == NULL) {
            //perror("Failed to allocate memory for separatorChar");
            // functions to free memory at each level
            return NULL;
        }
        separatorChar[0] = cmdString[i]; // '|'
        separatorChar[1] = '\0';

        // Add atomicInstance and separatorChar to cmdGroup's arrays
        if (cmdGroup->atomicArrIndex >= initialSize) {
            // Reallocate memory if needed
            initialSize *= 2;
            cmdGroup->atomicArr = (struct atomic**)realloc(cmdGroup->atomicArr, initialSize * sizeof(struct atomic*));
            cmdGroup->separatorArr = (char**)realloc(cmdGroup->separatorArr, initialSize * sizeof(char*));
            if (cmdGroup->atomicArr == NULL || cmdGroup->separatorArr == NULL) {
                //perror("Failed to reallocate memory for atomicArr or separatorArr");
                // functions to free memory at each level
                return NULL;
            }
        }
        cmdGroup->atomicArr[cmdGroup->atomicArrIndex++] = atomicInstance;
        cmdGroup->separatorArr[cmdGroup->sepArrIndex++] = separatorChar;

        // Move to the next character after the separator
        i++;
    }
    return cmdGroup; // returns the completely filled in struct (array of atomics filled in)
}

bool checkCmdGroup(struct cmd_group* cmdGroup){

    // Reject if there are no atomics
    if (cmdGroup->atomicArrIndex == 0) {
        //printf("No atomics found in command group\n");
        cmdGroup->validity = false;
        return false;
    }

    // Check for empty atomics (e.g. "ls |", "| ls", "ls | | grep")
    for (int i = 0; i < cmdGroup->atomicArrIndex; i++) {
        struct atomic* atomic = cmdGroup->atomicArr[i];
        if (atomic->atomicString == NULL) {
            //printf("Atomic string is a null pointer in command group \n");
            cmdGroup->validity = false;
            return false;
        }
        if (strlen(atomic->atomicString) == 0) {
            //printf("Empty atomic found at position %d in command group '%s'\n", i, cmdGroup->cmdString);
            cmdGroup->validity = false;
            return false;
        }
    }

    // Reject if last separator is a pipe
    if (strcmp(cmdGroup->separatorArr[cmdGroup->sepArrIndex-1],"|")==0){
        //printf("ERROR checkCmdGroup: Separator | found after last atomic group\n");
        cmdGroup->validity = false;
        return false;
    }

    // Check for each atomic's validity:
    for (int i=0; i<cmdGroup->atomicArrIndex; i++){
        struct atomic* atomic = cmdGroup->atomicArr[i];
        if (!checkAtomic(atomic)) {
            //printf("Invalid atomic at position %d in command group '%s'\n", i, cmdGroup->cmdString);
            cmdGroup->validity = false;
            return false;
        }
    }

    cmdGroup->validity = true;
    return true;
}


/*
    for each atomic in atomicArr, take the string and build the terminal and
    separator arrays.
*/
struct atomic* tokenizeAtomic(struct atomic* atomicGroup){
    ////////////////////////////// LLM GENERATED CODE BEGNS /////////////////////////////////

    // Initialize indices and validity
    atomicGroup->termArrIndex = 0;
    atomicGroup->sepArrIndex = 0;
    atomicGroup->validity = false;

    // Allocate initial memory for terminalArr and separatorArr
    int initialSize = 10; // Initial size, can be adjusted
    atomicGroup->terminalArr = (struct terminal**)malloc(initialSize * sizeof(struct terminal*));
    atomicGroup->separatorArr = (char**)malloc(initialSize * sizeof(char*));
    if (atomicGroup->terminalArr == NULL || atomicGroup->separatorArr == NULL) {
        //perror("Failed to allocate memory for terminalArr or separatorArr");
        // functions to free memory properly
        return NULL;
    }
    
    char* atomicString = atomicGroup->atomicString;
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
        struct terminal* terminalInstance = (struct terminal*)malloc(sizeof(struct terminal));
        terminalInstance->terminalString = (char*)malloc((termInstanceLength + 1) * sizeof(char));
        if (terminalInstance == NULL) {
            //perror("Failed to allocate memory for terminalInstance");
            // functions to free memory at each level
            return NULL;
        }
        strncpy(terminalInstance->terminalString, &atomicString[termInstanceStart], termInstanceLength);
        terminalInstance->terminalString[termInstanceLength] = '\0'; // null-terminate the string


        char* separatorChar = (char*)malloc(2 * sizeof(char));
        if (separatorChar == NULL) {
            //perror("Failed to allocate memory for separatorChar");
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
            atomicGroup->terminalArr = (struct terminal**)realloc(atomicGroup->terminalArr, initialSize * sizeof(struct terminal*));
            atomicGroup->separatorArr = (char**)realloc(atomicGroup->separatorArr, initialSize * sizeof(char*));
            if (atomicGroup->terminalArr == NULL || atomicGroup->separatorArr == NULL) {
                //perror("Failed to reallocate memory for terminalArr or separatorArr");
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

    // Redundant - already checked this in one level above
    if (strcmp(atomicGroup->separatorArr[atomicGroup->sepArrIndex-1], "|") == 0){
        return false; // Last separator should be empty
    }

    atomicGroup->validity = true;
    return true;
}


struct terminal* tokenizeTerminal(struct terminal* terminalGroup){
    char* terminalString = terminalGroup->terminalString;
    int length = strlen(terminalString);

    terminalGroup->cmdAndArgs = (char**)malloc(10 * sizeof(char*)); // Initial size
    terminalGroup->cmdAndArgsIndex = 0;
    if (terminalGroup->cmdAndArgs == NULL) {
        //perror("Failed to allocate memory for cmdAndArgs");
        return NULL;
    }

    for (int i = 0; i < length; i++) {
        if (isWhitespace(terminalString[i])) {
            // Skip prefixed excess whitespace before processing cmd/arg
            continue;
        }
        // Here you can add more logic to tokenize the terminal string if needed
        int tokenStart = i; // ends when we see a whitespace
        while (i < length && !isWhitespace(terminalString[i])) {
            i++;
        }
        int tokenLength = i - tokenStart;
        char* token = (char*)malloc((tokenLength + 1) * sizeof(char));
        if (token == NULL) {
            //perror("Failed to allocate memory for token");
            return NULL;
        }
        strncpy(token, &terminalString[tokenStart], tokenLength);
        token[tokenLength] = '\0'; // Null-terminate the string

        // Store the token in terminalGroup
        if (terminalGroup->cmdAndArgsIndex >= 10) {
            // Reallocate if needed
            terminalGroup->cmdAndArgs = (char**)realloc(terminalGroup->cmdAndArgs, (terminalGroup->cmdAndArgsIndex + 10) * sizeof(char*));
            if (terminalGroup->cmdAndArgs == NULL) {
                //perror("Failed to reallocate memory for cmdAndArgs array");
                return NULL;
            }
        }

        terminalGroup->cmdAndArgs[terminalGroup->cmdAndArgsIndex++] = token;
    }
    terminalGroup->cmdAndArgs[terminalGroup->cmdAndArgsIndex] = NULL; // Null-terminate the array of strings
    return terminalGroup;
}

bool checkTerminals(struct terminal* terminalStruct){
    // should not be empty or just whitespace
    for (int i = 0; i < terminalStruct->cmdAndArgsIndex; i++) {
        char* terminal = terminalStruct->cmdAndArgs[i];
        if (terminal == NULL || strlen(terminal) == 0) return false;
        // should be in the scanset of valid characters r"[^|;&<>]+"
        int length = strlen(terminal);
        for (int i = 0; i < length; i++) {
            char c = terminal[i];
            if (c == '|' || c == ';' || c == '&' || c == '<' || c == '>') {
                return false;
            }
        }
    }
    
    terminalStruct->validity = true;
    return true;
}


void testTokenizeShellCommand() {
    char* testCommand = "ls -l; echo Hello & pwd";
    struct shell_cmd* result = tokenizeShellCommand(testCommand);

    if (result == NULL) {
        //printf("Tokenization failed.\n");
        return;
    }

    //printf("Number of command groups: %d\n", result->cmdArrIndex);
    for (int i = 0; i < result->cmdArrIndex; i++) {
        //printf("Command Group %d: %s\n", i + 1, result->cmdGroupArr[i]->cmdString);
        if (i < result->sepArrIndex) {
            //printf("Separator %d: %s\n", i + 1, result->separatorArr[i]);
        }
    }

    // Free allocated memory (not shown here for brevity)
}

void testTokenizeCmdGroup() {
    struct cmd_group* cmdGroup = (struct cmd_group*)malloc(sizeof(struct cmd_group));
    char* testCmdGroup = "ls -l  >> name.txt | grep txt < name.txt | wc -l";
    cmdGroup->cmdString = testCmdGroup;
    cmdGroup = tokenizeCmdGroup(cmdGroup);

    if (cmdGroup == NULL) {
        //printf("Tokenization failed.\n");
        return;
    }

    //printf("Number of atomic commands: %d\n", cmdGroup->atomicArrIndex);
    for (int i = 0; i < cmdGroup->atomicArrIndex; i++) {
        //printf("Atomic Command %d: %s\n", i + 1, cmdGroup->atomicArr[i]->atomicString);
        if (i < cmdGroup->sepArrIndex) {
            //printf("Separator %d: %s\n", i + 1, cmdGroup->separatorArr[i]);
        }
    }

    // Free allocated memory (not shown here for brevity)
}

void testTokenizeShellAndCmdGroup() {
    char* testCommand = "ls -l; echo Hello & pwd; ls -l  >> name.txt | grep txt < name.txt | wc -l &";
    struct shell_cmd* shellResult = tokenizeShellCommand(testCommand);

    if (shellResult == NULL) {
        //printf("Shell command tokenization failed.\n");
        return;
    }

    //printf("Number of command groups: %d\n", shellResult->cmdArrIndex);
    for (int i = 0; i < shellResult->cmdArrIndex; i++) {
        struct cmd_group* cmdGroup = shellResult->cmdGroupArr[i];
        //printf("Command Group %d: %s\n", i + 1, cmdGroup->cmdString);

        struct cmd_group* cmdGroupResult = tokenizeCmdGroup(cmdGroup);
        if (cmdGroupResult == NULL) {
            //printf("Cmd group tokenization failed for command group %d.\n", i + 1);
            continue;
        }

        //printf("  Number of atomic commands: %d\n", cmdGroupResult->atomicArrIndex);
        for (int j = 0; j < cmdGroupResult->atomicArrIndex; j++) {
            //printf("  Atomic Command %d: %s\n", j + 1, cmdGroupResult->atomicArr[j]->atomicString);
            if (j < cmdGroupResult->sepArrIndex) {
                //printf("  Separator %d: %s\n", j + 1, cmdGroupResult->separatorArr[j]);
            }
        }

        // Free allocated memory for cmdGroupResult (not shown here for brevity)
    }

    // Free allocated memory for shellResult (not shown here for brevity)
}

void testTokenizeAtomic() {
    char* testAtomic = "ls -l  >> name.txt";
    struct atomic* result = (struct atomic*)malloc(sizeof(struct atomic));
    result->atomicString = testAtomic;
    result = tokenizeAtomic(result);

    if (result == NULL) {
        //printf("Tokenization failed.\n");
        return;
    }

    //printf("Number of terminals: %d\n", result->termArrIndex);
    for (int i = 0; i < result->termArrIndex; i++) {
        //printf("Terminal %d: %s\n", i + 1, result->terminalArr[i]->terminalString);
        if (i < result->sepArrIndex) {
            //printf("Separator %d: %s\n", i + 1, result->separatorArr[i]);
        }
    }

    // Free allocated memory (not shown here for brevity)
}

void printIndent(int level) {
    for (int i = 0; i < level; i++) printf("  ");
}

void testAllTokenizers(){
    char* shell_cmd = "cat file.txt | grep error > errors.txt; hop src - ..";
    //printf("{\n");
    //printf("  \"shell_cmd\": \"%s\",\n", shell_cmd);

    struct shell_cmd* shellResult = tokenizeShellCommand(shell_cmd);

    if (shellResult == NULL) {
        //printf("  \"error\": \"Shell command tokenization failed.\"\n}\n");
        return;
    }

    //printf("  \"command_groups\": [\n");
    for (int i = 0; i < shellResult->cmdArrIndex; i++) {
        printIndent(2);
        printf("{\n");
        printIndent(3);
        //printf("\"cmd_group_string\": \"%s\",\n", shellResult->cmdGroupArr[i]->cmdString);
        if (i < shellResult->sepArrIndex) {
            printIndent(3);
            //printf("\"separator\": \"%s\",\n", shellResult->separatorArr[i]);
        }

        struct cmd_group* cmdGroupResult = tokenizeCmdGroup(shellResult->cmdGroupArr[i]);
        if (cmdGroupResult == NULL) {
            printIndent(3);
            //printf("\"error\": \"Cmd group tokenization failed\"\n");
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

            struct atomic* atomicResult = tokenizeAtomic(cmdGroupResult->atomicArr[j]);
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
                struct terminal* terminalResult = tokenizeTerminal(atomicResult->terminalArr[k]);
                if (terminalResult == NULL) {
                    printf("{ \"error\": \"Terminal tokenization failed\" }%s\n", (k == atomicResult->termArrIndex - 1) ? "" : ",");
                    continue;
                }
                printf("{ \"terminal\": \"%s\"", terminalResult->terminalString);
                if (k < atomicResult->sepArrIndex && atomicResult->separatorArr[k][0] != '\0') {
                    printf(", \"separator\": \"%s\"", atomicResult->separatorArr[k]);
                }
                printf(", \"cmdAndArgs\": [");
                for (int m = 0; m < terminalResult->cmdAndArgsIndex; m++) {
                    printf("\"%s\"%s", terminalResult->cmdAndArgs[m], (m == terminalResult->cmdAndArgsIndex - 1) ? "" : ", ");
                }
                printf("] }%s\n", (k == atomicResult->termArrIndex - 1) ? "" : ",");
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

struct shell_cmd* verifyCommand(char* inputCommand){
    // Generate and fill in a shell_cmd struct for this inputCommand
    struct shell_cmd* shellResult = tokenizeShellCommand(inputCommand);
    if (shellResult == NULL) {
        //printf("Shell command tokenization failed.\n");
        return NULL;
    }

    for (int i = 0; i < shellResult->cmdArrIndex; i++) {
        //printf("Tokenizing command group %d of %d: %s\n", i+1, shellResult->cmdArrIndex, shellResult->cmdGroupArr[i]->cmdString);
        
        // For each cmd_group in shell_cmd's cmd_group array, tokenize it into its atomics (fill in this cmd_group struct fully with its atomic arrays)
        // tokenize shell command function only put cmd_strings into cmd_group structs -> tokenize further and fill in the atomics array
        struct cmd_group* cmdGroupResult = tokenizeCmdGroup(shellResult->cmdGroupArr[i]);
        if (cmdGroupResult == NULL) {
            //printf("Cmd group tokenization failed for command group %d.\n", i + 1);
            return shellResult;
        }
        
        shellResult->cmdGroupArr[i] = cmdGroupResult; // redundant?

        // Take this filled in cmd_group, and for each atomic in the atomic array, tokenize it further down
        for (int j = 0; j < cmdGroupResult->atomicArrIndex; j++) {
            struct atomic* atomicResult = tokenizeAtomic(cmdGroupResult->atomicArr[j]);
            if (atomicResult == NULL) {
                //printf("Atomic tokenization failed for atomic command %d in command group %d.\n", j + 1, i + 1);
                return shellResult;
            }
            cmdGroupResult->atomicArr[j] = atomicResult;
            for (int k = 0; k < atomicResult->termArrIndex; k++) {
                struct terminal* terminalResult = tokenizeTerminal(atomicResult->terminalArr[k]);
                if (terminalResult == NULL) {
                    //printf("Terminal tokenization failed for terminal %d in atomic command %d of command group %d.\n", k + 1, j + 1, i + 1);
                    return shellResult;
                }
                atomicResult->terminalArr[k] = terminalResult; // Redundant???
            }
        }
    }



    bool valid = checkShellCmd(shellResult);

    if (valid) {
        //printf("Valid Syntax!\n");
    } else {
        printf("Invalid Syntax!\n");
    }
    return shellResult;

}

// int main() {
//     testAllTokenizers();
//     return 0;
// }