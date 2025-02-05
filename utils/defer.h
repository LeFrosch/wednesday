#pragma once

#include <stdlib.h>

typedef void* defer_t;

#define defer(fun, args) defer_t defer_## __COUNTER__ __attribute__((unused, __cleanup__(defer_## fun)) = (void*) &args;

static inline void defer_free(void* ptr) {
    free(ptr);
}
