#pragma once

#include "error.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void* arg;
    bool err;

    const char* file;
    const char* func;
    uint32_t line;
} defer_t;

#define defer(fun, args)                                                                                               \
    defer_t concat(defer_, __COUNTER__) __attribute__((unused, __cleanup__(defer_##fun))) = {                          \
        .arg = (void*)&args, .err = false, .line = __LINE__, .file = __FILE__, .func = __FUNCTION__                    \
    }

#define errdefer(fun, args)                                                                                            \
    defer_t concat(defer_, __COUNTER__) __attribute__((unused, __cleanup__(defer_##fun))) = {                          \
        .arg = (void*)&args, .err = true, .line = __LINE__, .file = __FILE__, .func = __FUNCTION__                     \
    }

#define enable_defer(fun, type, code)                                                                                  \
    __attribute__((unused)) static inline void defer_##fun(const defer_t* def) {                                       \
        if (!defer_should_run(def)) {                                                                                  \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        const uint32_t ret = fun(type(def->arg));                                                                      \
        if (ret != code) {                                                                                             \
            error_report(def->file, def->func, def->line, "defer fialed, expected: %d, got: %d", code, ret);           \
        }                                                                                                              \
    }                                                                                                                  \
    __attribute__((unused)) static int _defer_unused_variable_for_semicolon

__attribute__((unused)) static inline bool
defer_should_run(const defer_t* def) {
    return !def->err || error_get_code() != 0;
}
