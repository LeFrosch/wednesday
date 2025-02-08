#include "io.h"
#include "snow.h"
#include "utils/test.h"
#include "utils/utils.h"

describe(io) {
    before_each() {
        errors_clear();
    }

    it("error when parameter is invalid") {
        assert(!create_file("file", NULL));

        file_t file;
        assert(!create_file(NULL, &file));

        asserteq(errors_get_count(), 2);
    }

    it("can create a new file") {
        file_t file;
        assertsuc(create_file("file", &file));

        assert(file.handle);
        assert(file.parent);
    }

    it("can create a new file in new directory") {
        file_t file;
        assertsuc(create_file("dir/dir/file", &file));

        assert(file.handle);
        assert(file.parent);
    }
}
