#include "parser.h"
#include "partB.h"

#include <sys/wait.h>
#include <fcntl.h>

extern pid_t mainPid;

extern int bg_fork; // Global variable to indicate background process
extern int pipe_exists;


void executeShellCommand(struct shell_cmd* shellCommandStruct);

void executeCmdGroup(struct cmd_group* cmdGroupStruct);

void executeAtomicCmd(struct atomic* atomicCmdStruct);

