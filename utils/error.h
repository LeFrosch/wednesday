#pragma once

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#define result_t __attribute__((warn_unused_result)) int32_t

#define SUCCESS 0

#define error_push(CODE, ...)                                                                                          \
    do {                                                                                                               \
        errors_push_new(__FILE__, __LINE__, __func__, CODE);                                                           \
        (void)(__VA_ARGS__);                                                                                           \
    } while (0)

#define failure(CODE, ...)                                                                                             \
    do {                                                                                                               \
        error_push(CODE, __VA_ARGS__);                                                                                 \
        return CODE;                                                                                                   \
    } while (0)

#define ensure(CALL, ...)                                                                                              \
    do {                                                                                                               \
        if (!CALL) {                                                                                                   \
            failed(EINVAL, msg(#CALL " "), ##__VA_ARGS__);                                                             \
        }                                                                                                              \
    } while (0)

#define msg(MSG) errors_append_message(": " MSG)

#define with(EXPR, FORMAT) errors_append_message(", " #EXPR " = " FORMAT, EXPR)

void errors_push_new(const char* file, uint32_t line, const char* func, int32_t code);

__attribute__((__format__(__printf__, 1, 2))) void errors_append_message(const char* format, ...);

void errors_print(void);

void errors_clear(void);

size_t errors_get_count(void);

result_t* errors_get_results(void);

const char** errors_get_messages(void);
