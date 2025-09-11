#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

// Logging
FILE *open_log_file(const char *role);
void close_log_file(void);
void log_event(const char *fmt, ...);
int logging_enabled(void);

// Time helpers
uint64_t now_ms(void);

// Random drop
int should_drop(double loss_rate);

#endif // COMMON_H
