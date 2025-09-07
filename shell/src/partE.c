#include "../include/partE.h"

struct job* jobListHead = NULL;
struct job* jobListTail = NULL;


void addJob(pid_t pid, char* commandString) {
    struct job* newJob = malloc(sizeof(struct job));
    newJob->pid = pid;
    newJob->command = strdup(commandString);
    newJob->running = 1; // Job is running
    newJob->nextJob = NULL;

    if (jobListHead == NULL) {
        // If the list is empty, the new job is both the head and tail
        jobListHead = newJob;
        jobListTail = newJob;
    } else {
        // Otherwise, append the new job to the end of the list
        jobListTail->nextJob = newJob;
        jobListTail = newJob;
    }
}
