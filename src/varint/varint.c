#include "varint.h"

size_t
varint_get(const unsigned char* buf, uint64_t* out) {
    uint64_t val = 0;
    int n = 0;
    unsigned char c = 0;

    do {
        c = buf[n++];
        val = (val << 7) | (c & 0x7f);
    } while ((c & 0x80) != 0 && n < 8);

    // read the full last byte if used
    if (c & 0x80) {
        val = (val << 8) | buf[8];
        n = 9;
    }

    *out = val;
    return n;
}

size_t
varint_put(unsigned char* buf, uint64_t val) {
    unsigned char tmp[9];

    // use the full last byte if 9 bytes are required to encode the number
    if (val & 0xff00000000000000) {
        tmp[0] = val;
        val >>= 8;
    } else {
        tmp[0] = val & 0x7f;
        val >>= 7;
    }

    int n = 1;
    while (val != 0 && n < 9) {
        tmp[n++] = (val & 0x7f) | 0x80;
        val >>= 7;
    }

    for (int i = n - 1; i >= 0; i--) {
        buf[n - i - 1] = tmp[i];
    }

    return n;
}

size_t
varint_len(uint64_t val) {
    int len = 0;
    do {
        len++;
        val >>= 7;
    } while (val != 0 && len < 9);

    return len;
}
