#include "io.h"
#include "snow.h"
#include "utils/utils.h"

describe(io) {
    before_each() {
        errors_clear();
    }

    it("can create a new file") {
        file_t file;
        assert(create_file("file", &file));

        assert(file.handle);
        assert(file.parent);
    }

    it("can create a new file in new directory") {
        file_t file;
        assert(create_file("dir/dir/file", &file));

        assert(file.handle);
        assert(file.parent);
    }
}
