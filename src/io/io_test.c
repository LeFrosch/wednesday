#include "io.h"
#include "snow.h"
#include "utils/test.h"
#include "utils/utils.h"

describe(io) {
    before_each() {
        errors_clear();
    }

    it("error when parameter is invalid") {
        assert(!file_open("file", NULL));

        file_t file;
        assert(!file_open(NULL, &file));

        asserteq(errors_get_count(), 2);
    }

    it("can create a new file") {
        file_t file;
        assertsuc(file_open("file", &file));
        assertsuc(file_close(&file));

        assert(file.handle);
        assert(file.parent);
    }

    it("can create a new file in new directory") {
        file_t file;
        assertsuc(file_open("dir/dir/file", &file));
        assertsuc(file_close(&file));

        assert(file.handle);
        assert(file.parent);
    }
}
