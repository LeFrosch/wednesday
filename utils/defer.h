#pragma once

#include "error.h"

#include <stdbool.h>

typedef struct defer {
    void* arg;
    const bool on_error;
} defer_t;

#define defer(fun, args)                                                                                               \
    defer_t defer_##__COUNTER__                                                                                        \
        __attribute__((unused, __cleanup__(defer_##fun))) = {.arg = (void*)&args, .on_error = false}

#define errdefer(fun, args)                                                                                            \
    defer_t defer_##__COUNTER__                                                                                        \
        __attribute__((unused, __cleanup__(defer_##fun))) = {.arg = (void*)&args, .on_error = true}

#define enabel_defer(fun)                                                                                              \
    static inline void defer_##fun(const defer_t* def) {                                                               \
        if (!defer_should_run(def)) {                                                                                  \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        if (!fun(def->arg)) {                                                                                          \
            error_push(EINVAL, msg("failure in " #fun));                                                               \
        }                                                                                                              \
    }                                                                                                                  \
    int _defer_unused_variable_for_semicolon_##__COUNTER__

static inline bool defer_should_run(const defer_t* def) {
    return !def->on_error || errors_get_count() > 0;
}

static inline void defer_free(const defer_t* def) {
    if (defer_should_run(def)) {
        free(def->arg);
    }
}
