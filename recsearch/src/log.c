#include "log.h"

void log_printf(const char *format,...) {
    va_list argptr;

    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}
