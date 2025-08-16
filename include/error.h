#pragma once

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#define result_t __attribute__((warn_unused_result)) int32_t

#define SUCCESS 0
#define FAILURE 1

typedef struct {
    const char* file;
    const char* func;
    uint32_t line;

    int32_t code;
    char msg[128];
} error_frame_t;

/// Reports a thread local error.
__attribute__((__format__(__printf__, 5, 0))) void
error_report(const char* file, const char* func, uint32_t line, int32_t code, const char* format, ...);

/// Returns the last error code reported by the current thread.
int32_t
error_get_code(void);

/// Returns the length of the error trace for the current thread.
uint32_t
error_trace_length(void);

/// Gets the nth frame of the error trace or null if the index is out of bounds.
error_frame_t*
error_trace_nth(uint32_t nth);

/// Clears the error state and the trace for the current thread.
void
error_clear(void);

#define failure(code, format, ...)                                                                                     \
    do {                                                                                                               \
        error_report(__FILE__, __FUNCTION__, __LINE__, code, format, #__VA_ARGS__);                                    \
        return FAILURE;                                                                                                \
    } while (0)

#define ensure(expr)                                                                                                   \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            error_report(__FILE__, __FUNCTION__, __LINE__, EINVAL, "violation: %s", #expr);                            \
            return FAILURE;                                                                                            \
        }                                                                                                              \
    } while (0)
