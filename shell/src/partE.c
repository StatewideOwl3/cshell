#include "../include/partE.h"
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

static char* dup_trimmed(const char* src) {
    if (src == NULL) src = "";
    while (*src && isspace((unsigned char)*src)) src++;
    const char* end = src + strlen(src);
    while (end > src && isspace((unsigned char)*(end - 1))) end--;
    size_t len = (size_t)(end - src);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, src, len);
    out[len] = '\0';
    return out;
}

struct job* job_list = NULL;
struct job* jobListTail = NULL;


// Add a job to the list
void addJob(pid_t pid, char *cmd, int running) {
    //printf("adding job : %d %s %d", pid, cmd, running);
    struct job *new_job = malloc(sizeof(struct job));
    new_job->pid = pid;
    const char* base = cmd ? cmd : "";
    char* trimmed = dup_trimmed(base);
    if (!trimmed) {
        free(new_job);
        return;
    }
    new_job->command = trimmed;
    new_job->running = running;
    new_job->next = job_list;
    job_list = new_job;
}

// Remove a job from the list (when terminated)
void removeJob(pid_t pid) {
    struct job **curr = &job_list;
    while (*curr) {
        if ((*curr)->pid == pid) {
            struct job *tmp = *curr;
            *curr = (*curr)->next;
            free(tmp->command);
            free(tmp);
            return;
        }
        curr = &((*curr)->next);
    }
}

// Update job states using waitpid
void updateJobs() {
    struct job *j = job_list;
    int status;
    while (j) {
        // Avoid reaping background jobs that are managed by check_bg_jobs(), so
        // that their termination message is printed there. If we called waitpid
        // here first, we'd consume the exit status and suppress that message.
        if (is_bg_job_running(j->pid)) {
            j = j->next;
            continue;
        }

        pid_t result = waitpid(j->pid, &status, WNOHANG | WUNTRACED);
        if (result == 0) {
            // Still running / nothing to report
            j = j->next;
            continue;
        }
        if (result > 0) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // Terminated: remove from list
                pid_t dead = j->pid;
                j = j->next; // advance before removal
                removeJob(dead);
                continue;
            } else if (WIFSTOPPED(status)) {
                j->running = 0; // stopped but retained
            }
#ifdef WCONTINUED
            else if (WIFCONTINUED(status)) {
                j->running = 1; // continued
            }
#endif
        } else { // result < 0
            if (errno == ECHILD) {
                // Already reaped elsewhere (e.g., separate bg check); remove stale entry
                pid_t dead = j->pid;
                j = j->next;
                removeJob(dead);
                continue;
            }
        }
        j = j->next;
    }
}

int compareJobs(const void *a, const void *b) {
    struct job *ja = *(struct job **)a;
    struct job *jb = *(struct job **)b;
    return strcmp(ja->command, jb->command);
}

void printActivities() {
    // First reap any background jobs so we don't show them as Running right
    // after they have actually exited (race between user invoking activities
    // and the periodic check in the main loop).
    check_bg_jobs();
    updateJobs(); // refresh states (non-bg jobs or any still present)

    // Count jobs
    int count = 0;
    struct job *j = job_list;
    while (j) { count++; j = j->next; }

    if (count == 0) {
        //printf("No active jobs\n");
        return;
    }

    // Copy into array
    struct job **arr = malloc(count * sizeof(struct job *));
    j = job_list;
    for (int i = 0; i < count; i++) {
        arr[i] = j;
        j = j->next;
    }

    // Sort by command
    qsort(arr, count, sizeof(struct job *), compareJobs);

    // Print
    for (int i = 0; i < count; i++) {
        printf("[%d] : %s - %s\n", 
               arr[i]->pid,
               arr[i]->command,
               arr[i]->running ? "Running" : "Stopped");
    }

    free(arr);
}



