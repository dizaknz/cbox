#ifndef _LOG_H
#define _LOG_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef debug
#define debug 1
#endif

void log_printf(const char *,...);

#define LOG_ERROR(args)               \
{                                     \
    fprintf(stderr, "ERROR      : "); \
    log_printf args;                  \
}

#define LOG_DEBUG(args)                   \
{                                         \
    if (debug) {                          \
        fprintf(stderr, "DEBUG      : "); \
        log_printf args;                  \
    }                                     \
}

#define LOG_INFO(args)                \
{                                     \
    fprintf(stderr, "INFO       : "); \
    log_printf args;                  \
}
#endif
