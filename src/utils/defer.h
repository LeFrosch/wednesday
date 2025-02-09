#pragma once

#include "utils/error.h"

#include <stdbool.h>

typedef struct {
    void* arg;
    bool on_error;

    // only enable them in debug mode?
    int line;
    const char* file;
} defer_t;

// workaround to ensure the __COUNTER__ macro is expanded before the concatenation
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

#define defer(fun, args)                                                                                               \
    defer_t CONCAT(defer_, __COUNTER__) __attribute__((unused, __cleanup__(defer_##fun))) = {                          \
        .arg = (void*)&args, .on_error = false, .line = __LINE__, .file = __FILE__                                     \
    }

#define errdefer(fun, args)                                                                                            \
    defer_t CONCAT(defer_, __COUNTER__) __attribute__((unused, __cleanup__(defer_##fun))) = {                          \
        .arg = (void*)&args, .on_error = true, .line = __LINE__, .file = __FILE__                                      \
    }

#define enabel_defer(fun, type, code)                                                                                  \
    __attribute__((unused)) static inline void defer_##fun(const defer_t* def) {                                       \
        if (!defer_should_run(def)) {                                                                                  \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        if (fun(type(def->arg)) != code) {                                                                             \
            errors_push_new(def->file, def->line, def->on_error ? "errdefer" : "defer", errno ? errno : EINVAL);       \
        }                                                                                                              \
    }                                                                                                                  \
    int _defer_unused_variable_for_semicolon

__attribute__((unused)) static inline bool
defer_should_run(const defer_t* def) {
    return !def->on_error || errors_get_count() > 0;
}
