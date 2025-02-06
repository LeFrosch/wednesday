#pragma once

#include "utils/error.h"

#include <stdbool.h>

typedef struct {
    void* arg;
    bool on_error;
} defer_t;

// workaround to ensure the __COUNTER__ macro is expanded before the concatenation
#define concat_(a, b) a##b
#define concat(a, b) concat_(a, b)

#define defer(fun, args)                                                                                               \
    defer_t concat(defer_, __COUNTER__)                                                                                \
        __attribute__((unused, __cleanup__(defer_##fun))) = {.arg = (void*)&args, .on_error = false}

#define errdefer(fun, args)                                                                                            \
    defer_t concat(defer_, __COUNTER__)                                                                                \
        __attribute__((unused, __cleanup__(defer_##fun))) = {.arg = (void*)&args, .on_error = true}

#define enabel_defer(fun, type)                                                                                        \
    __attribute__((unused)) static inline void defer_##fun(const defer_t* def) {                                       \
        if (!defer_should_run(def)) {                                                                                  \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        if (!fun(type(def->arg))) {                                                                                    \
            error_push(EINVAL, msg("failure in " #fun));                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    int _defer_unused_variable_for_semicolon_##__COUNTER__

__attribute__((unused)) static inline bool defer_should_run(const defer_t* def) {
    return !def->on_error || errors_get_count() > 0;
}
