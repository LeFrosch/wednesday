#include "snow.h"
#include "utils/test.h"
#include "varint.h"

static void
test_varint(const uint64_t val, const size_t size) {
    asserteq(varint_len(val), size);

    unsigned char buf[10];
    asserteq(varint_put(buf, val), size);

    uint64_t out;
    asserteq(varint_get(buf, &out), size);

    asserteq(out, val);
}

describe(varint) {
    it("can encode 0") {
        test_varint(0, 1);
    }

    it("can encode 1") {
        test_varint(1, 1);
    }

    it("can encode 1 << 8") {
        test_varint(1 << 8, 2);
    }

    it("can encode UINT64_MAX") {
        test_varint(UINT64_MAX, 9);
    }
}
