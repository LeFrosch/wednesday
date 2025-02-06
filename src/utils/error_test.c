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
        asserteq(errors_get_results()[0], EIO);
    }

    it("can record at most 32 errors") {
        for (size_t i = 0; i < 100; i++) {
            error_push(EIO, msg("test error"));
        }

        asserteq(errors_get_count(), 32);
    }

    it("translates error code") {
        error_push(EIO, msg("test error"));
        assert(strstr(errors_get_messages()[0], "Input/output error"));
    }
}
