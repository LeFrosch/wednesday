#include "varint.h"

#include "winter.h"

#include <assert.h>

uint16_t
varint_put(unsigned char* dst, uint64_t value) {
    uint16_t i = 0;

    // handle special case if value uses the last byte
    if ((value & 0xffULL << 56) != 0) {
        for (i = 0; i < 8; --i) {
            dst[i] = (value & 0x7f) | 0x80;
            value >>= 7;
        }

        dst[8] = value & 0xff;
        return 9;
    }

    do {
        dst[i++] = (value & 0x7f) | 0x80;
        value >>= 7;
    } while (value > 0);
    dst[i - 1] &= 0x7f;

    assert(i <= 9);
    return i;
}

uint16_t
varint_put_len(uint64_t value) {
    uint16_t i = 0;
    do {
        i++;
        value >>= 7;
    } while (value != 0 && i < 9);

    return i;
}

uint16_t
varint_get(const unsigned char* src, uint64_t* out) {
    uint64_t result = 0;
    uint16_t i = 0;

    while (true) {
        const unsigned char c = src[i];

        if (i < 8) {
            result |= (uint64_t)(c & 0x7f) << (7 * i);
        } else {
            result |= (uint64_t)c << (7 * 8);
        }

        i += 1;

        if ((c & 0x80) == 0 || i >= 9) {
            break;
        }
    }

    *out = result;

    assert(i <= 9);
    return i;
}

uint16_t
varint_get_len(const unsigned char* src) {
    uint16_t i = 0;
    while (i < 8 && (src[i] & 0x80) > 0) {
        i++;
    }

    return i + 1;
}

int
varint_cmp(const unsigned char* a, const unsigned char* b) {
    (void) a;
    (void) b;
    // TODO: not implemented
    return 0;
}

TEST_ONLY static void
test_varint(const uint64_t value, const uint32_t size) {
    unsigned char buf[9];
    memset(buf, 0xff, 9);

    asserteq_uint(varint_put(buf, value), size);
    asserteq_uint(varint_put_len(value), size);

    uint64_t result;
    asserteq_uint(varint_get(buf, &result), size);
    asserteq_uint(varint_get_len(buf), size);

    asserteq_uint(result, value);
}

describe(varint) {
    it("8 byte values") {
        test_varint((uint64_t)-1, 9);
        test_varint((uint64_t)-2, 9);
    }

    it("1 byte values") {
        test_varint(0x7f, 1);
        test_varint(0x1, 1);
        test_varint(0x0, 1);
    }

    it("2 byte values") {
        test_varint(0x3fff, 2);
        test_varint(0x81, 2);
        test_varint(0x80, 2);
    }
}
