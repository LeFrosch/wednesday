#include "error.h"
#include "snow.h"

describe(error) {
    before_each() {
        errors_clear();
    }

    it("no initial errors") {
        asserteq(errors_get_count(), 0);
    }

    it("can record error") {
        error_push(EIO, msg("test error"));
        asserteq(errors_get_count(), 1);
    }

    it("can record at most 32 errors") {
        for (size_t i = 0; i < 100; i++) {
            error_push(EIO, msg("test error"));
        }

        asserteq(errors_get_count(), 32);
    }

    it("translates error code") {
        error_push(EIO, msg("test error"));

        const char* msg = errors_get_message();
        assertneq(msg, NULL);

        assert(strstr(msg, "Input/output error"));
    }

    it("trace contains all messages") {
        error_push(EIO, msg("test error 1"));
        error_push(EIO, msg("test error 2"));
        error_push(EIO, msg("test error 3"));

        const char* trace = errors_get_trace();
        assertneq(trace, NULL);

        assert(strstr(trace, "error 1"));
        assert(strstr(trace, "error 2"));
        assert(strstr(trace, "error 3"));
    }
}
