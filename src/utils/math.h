#pragma once

#include "utils/error.h"

static inline result_t
overflow_failure(
  const char* file,
  const uint32_t line,
  const char* func,
  const char* msg,
  const size_t a,
  const size_t b
) {
    errors_push_new(file, line, func, EDOM);
    errors_append_message(": %s", msg);
    errors_append_message(", a = %lu", a);
    errors_append_message(", b = %lu", b);

    return FAILURE;
}

#define ckd_add(a, b, res)                                                                                             \
    (__builtin_add_overflow(a, b, res)                                                                                 \
       ? overflow_failure(__FILE__, __LINE__, __func__, "add overflowd: " #a " + " #b, a, b)                           \
       : SUCCESS)

#define ckd_sub(a, b, res)                                                                                             \
    (__builtin_sub_overflow(a, b, res)                                                                                 \
       ? overflow_failure(__FILE__, __LINE__, __func__, "sub overflowd: " #a " - " #b, a, b)                           \
       : SUCCESS)
