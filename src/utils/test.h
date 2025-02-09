#pragma once

#include "snow.h"
#include "utils.h"

#define assertsuc(call)                                                                                                \
    do {                                                                                                               \
        if (errors_get_count() > 0) {                                                                                  \
            fail("Unexpected error before call: %s", errors_get_message());                                            \
        }                                                                                                              \
                                                                                                                       \
        if (!(call)) {                                                                                                 \
            const char* msg = errors_get_message();                                                                    \
            assertneq(msg, NULL, "Expected an error to be recorded on failure");                                       \
                                                                                                                       \
            fail("Unexpected error: %s", msg);                                                                         \
        }                                                                                                              \
    } while (0)

#define asserterr(call)                                                                                                \
    do {                                                                                                               \
        if (errors_get_count() > 0) {                                                                                  \
            fail("Unexpected error before call: %s", errors_get_message());                                            \
        }                                                                                                              \
                                                                                                                       \
        asserteq(call, FAILURE);                                                                                     \
                                                                                                                       \
        if (errors_get_count() == 0) {                                                                                 \
            fail("Expected an error");                                                                                 \
        }                                                                                                              \
    } while (0)
