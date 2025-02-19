#include "io.h"
#include "snow.h"
#include "utils/test.h"

// workaround to ensure the __LINE__ macro is expanded
#define STRINGIFY_(a) #a
#define STRINGIFY(a) STRINGIFY_(a)

#define unique_file STRINGIFY(__LINE__)

describe(io) {
    before_each() {
        errors_clear();
    }

    it("error when parameter is invalid") {
        assert(!file_open(NULL, "file"));

        file_t file;
        assert(!file_open(&file, NULL));

        asserteq(errors_get_count(), 2);
    }

    it("can create a new file") {
        file_t file;
        assertsuc(file_open(&file, unique_file));
        assertsuc(file_close(&file));

        assert(file.handle);
        assert(file.parent);
    }

    it("can create a new file in new directory") {
        file_t file;
        assertsuc(file_open(&file, "dir/dir/file"));
        assertsuc(file_close(&file));

        assert(file.handle);
        assert(file.parent);
    }

    it("can write and read from file") {
        file_t file;
        assertsuc(file_open(&file, unique_file));
        defer(file_close, file);

        const char* data = "test";
        const size_t size = strlen(data) + 1;
        assertsuc(file_write(&file, 0, data, size));

        char* read = alloca(size);
        assertsuc(file_read(&file, 0, read, size));

        asserteq_str(data, read);
    }

    it("error when reading after EOF") {
        file_t file;
        assertsuc(file_open(&file, unique_file));
        defer(file_close, file);

        char read[10];
        asserterr(file_read(&file, 0, read, sizeof(read)));
    }
}
