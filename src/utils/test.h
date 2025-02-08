#pragma once

#define assertsuc(CALL)                                                                                                \
    do {                                                                                                               \
        asserteq(errors_get_count(), 0, "an error was already recorded");                                              \
                                                                                                                       \
        if (!CALL) {                                                                                                   \
            const char* msg = errors_get_message();                                                                    \
            assertneq(msg, NULL, "Expected an error to be recorded on failure");                                       \
                                                                                                                       \
            fail("Unexpected error: %s", msg);                                                                         \
        }                                                                                                              \
    } while (0)
