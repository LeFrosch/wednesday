#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "error.h"
#include "util.h"

#define MAX_ERRORS 32

_Thread_local static error_frame_t error_list[MAX_ERRORS];
_Thread_local static uint32_t error_count = 0;
_Thread_local static int32_t error_code = 0;

__attribute__((__format__(__printf__, 5, 0))) void
error_report(const char* file, const char* func, const uint32_t line, const int32_t code, const char* format, ...) {
    assert(code != 0);

    error_code = code;

    if (error_count >= MAX_ERRORS) {
        return;
    }

    error_frame_t* frame = error_list + error_count++;
    frame->file = file;
    frame->func = func;
    frame->line = line;
    frame->code = code;

    va_list args;
    va_start(args, format);
    vsnprintf(frame->msg, sizeof(frame->msg), format, args);
    va_end(args);
}

int32_t
error_get_code(void) {
    return error_code;
}

uint32_t
error_trace_length(void) {
    return min(error_count, MAX_ERRORS);
}

void
error_clear(void) {
    error_code = 0;
    error_count = 0;
}

error_frame_t*
error_trace_nth(const uint32_t nth) {
    if (nth >= error_count || error_count >= MAX_ERRORS) {
        return NULL;
    }

    return error_list + nth;
}
