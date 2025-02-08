#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#define result_t __attribute__((warn_unused_result)) int32_t

#define SUCCESS 1
#define FAILURE 0

#define error_push(code, ...)                                                                                          \
    do {                                                                                                               \
        errors_push_new(__FILE__, __LINE__, __func__, code);                                                           \
        (void)(__VA_ARGS__);                                                                                           \
    } while (0)

#define failure(code, ...)                                                                                             \
    do {                                                                                                               \
        error_push(code, ##__VA_ARGS__);                                                                               \
        return FAILURE;                                                                                                \
    } while (0)

#define msg(msg) errors_append_message(": " msg)

#define with(expr, format) errors_append_message(", " #expr " = " format, expr)

#define ensure(call, ...)                                                                                              \
    do {                                                                                                               \
        if (!(call)) {                                                                                                 \
            failure(EINVAL, msg(#call " "), ##__VA_ARGS__);                                                            \
        }                                                                                                              \
    } while (0)

#define ensure_no_errors()                                                                                             \
    do {                                                                                                               \
        if (errors_get_count() > 0) {                                                                                  \
            failure(EINVAL, msg("expected no errors"));                                                                \
        }                                                                                                              \
    } while (0)

void
errors_push_new(const char* file, uint32_t line, const char* func, int32_t code);

__attribute__((__format__(__printf__, 1, 2))) void
errors_append_message(const char* format, ...);

void
errors_print(void);

void
errors_clear(void);

size_t
errors_get_count(void);

const char*
errors_get_message(void);
