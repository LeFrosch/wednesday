#pragma once

#include <stdio.h>
#include <unistd.h>

#include "utils/defer.h"
#include "utils/error.h"
#include "utils/math.h"

__attribute__((unused)) static inline void
defer_free(const defer_t* def) {
    if (defer_should_run(def)) {
        free(def->arg);
    }
}

enabel_defer(close, *(int*), 0);
