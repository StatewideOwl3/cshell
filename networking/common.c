#include "common.h"
#include <stdarg.h>

static FILE *g_log = NULL;
static int g_log_enabled = -1;

int logging_enabled(void) {
    if (g_log_enabled == -1) {
        const char *env = getenv("RUDP_LOG");
        g_log_enabled = (env && strcmp(env, "1") == 0) ? 1 : 0;
    }
    return g_log_enabled;
}

FILE *open_log_file(const char *role) {
    if (!logging_enabled()) return NULL;
    if (g_log) return g_log;
    const char *fname = NULL;
    if (strcmp(role, "server") == 0) fname = "server_log.txt";
    else fname = "client_log.txt";
    g_log = fopen(fname, "w");
    return g_log;
}

void close_log_file(void) {
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
}

void log_event(const char *fmt, ...) {
    if (!logging_enabled()) return;
    if (!g_log) return;

    char time_buffer[30];
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", localtime(&curtime));

    fprintf(g_log, "[%s.%06ld] [LOG] ", time_buffer, tv.tv_usec);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);

    fprintf(g_log, "\n");
    fflush(g_log);
}

uint64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

int should_drop(double loss_rate) {
    if (loss_rate <= 0.0) return 0;
    if (loss_rate >= 1.0) return 1;
    double r = (double)rand() / (double)RAND_MAX;
    return r < loss_rate;
}
