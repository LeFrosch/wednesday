#include "blob.h"

#include "util.h"
#include "varint.h"

int
blob_cmp(const blob_t a, const blob_t b) {
    const int ret = memcmp(a.data, b.data, min(a.size, b.size));
    if (ret == 0) {
        if (a.size < b.size) {
            return -1;
        }
        if (a.size > b.size) {
            return 1;
        }
    }

    return ret;
}

uint64_t
blob_put(unsigned char* dst, const blob_t blob) {
    const uint16_t vint_len = varint_put(dst, blob.size);
    memcpy(dst + vint_len, blob.data, blob.size);
    return vint_len + blob.size;
}

uint64_t
blob_put_len(blob_t blob) {
    return varint_put_len(blob.size) + blob.size;
}

uint64_t
blob_get(unsigned char* src, blob_t* out) {
    const uint16_t vint_len = varint_get(src, &out->size);
    out->data = src + vint_len;
    return vint_len + out->size;
}

uint64_t
blob_get_len(const unsigned char* src) {
    uint64_t blob_len;
    const uint16_t vint_len = varint_get(src, &blob_len);
    return vint_len + blob_len;
}

TEST_ONLY blob_t
blob_from_string(const char* value) {
    return (blob_t){
        .size = strlen(value) + 1,
        .data = (unsigned char*)value,
    };
}
