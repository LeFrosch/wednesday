#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#define result_t __attribute__((warn_unused_result)) int32_t

#define SUCCESS 0
#define FAILURE 1

/// Reports a thread local error.
__attribute__((__format__(__printf__, 5, 0))) void
error_report(const char* file, const char* func, uint32_t line, int32_t code, const char* format, ...);

/// Returns the first error code reported by the current thread.
int32_t
error_get_code(void);

/// Returns the number of errors reported by the current thread;
uint32_t
error_get_count(void);

/// Clears the error and the trace in debug mode.
void
error_clear(void);

/// Writes the error and the message to the file. Also writes the trace in
/// debug mode.
void
error_log(FILE* file);

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
