#include "defer.h"
#include "snow.h"

result_t
mark(int* ptr) {
    *ptr = 1;
    return SUCCESS;
}

enabel_defer(mark, (int*));

describe(defer) {
    before_each() {
        errors_clear();
    }

    it("defer works as expected") {
        int i = 0;
        {
            defer(mark, i);
        }
        asserteq(i, 1);
    }

    it("errdefer does not execute without an error") {
        int i = 0;
        {
            errdefer(mark, i);
        }
        asserteq(i, 0);
    }

    it("errdefer executes after an error") {
        int i = 0;
        {
            errdefer(mark, i);
            error_push(EIO, msg("test error"));
        }
        asserteq(i, 1);
    }
}
