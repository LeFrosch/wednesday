#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "error.h"
#include "util.h"

#define MAX_ERRORS 32

typedef struct {
    const char* file;
    const char* func;
    uint32_t line;

    int32_t code;
    char msg[128];
} error_t;

_Thread_local static error_t error_list[MAX_ERRORS];
_Thread_local static uint32_t error_count = 0;
_Thread_local static int32_t error_code = 0;

__attribute__((__format__(__printf__, 5, 0))) void
error_report(const char* file, const char* func, const uint32_t line, const int32_t code, const char* format, ...) {
    assert(code != 0);

    error_code = code;

    if (error_count >= MAX_ERRORS) {
        return;
    }

    error_t* error = error_list + error_count++;
    error->file = file;
    error->func = func;
    error->line = line;
    error->code = code;

    va_list args;
    va_start(args, format);
    vsnprintf(error->msg, sizeof(error->msg), format, args);
    va_end(args);
}

int32_t
error_get_code(void) {
    return error_code;
}

uint32_t
error_get_count(void) {
    return error_count;
}

void
error_clear(void) {
    error_code = 0;
    error_count = 0;
}

void
error_log(FILE* file) {
    if (error_count == 0) {
        return;
    }

    fprintf(file, "error log (%d) for %d\n ", error_count, error_code);

    for (uint32_t i = 0; i < min(error_count, MAX_ERRORS); ++i) {
        error_t* error = error_list + i;
        fprintf(file, "-> %s:%d in %s: (%d) %s\n", error->file, error->line, error->func, error->code, error->msg);
    }

    if (error_count >= MAX_ERRORS) {
        fprintf(file, "-> trace truncated (out of memory)\n");
    }
}
