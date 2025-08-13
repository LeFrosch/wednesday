#include "page.h"
#include "error.h"
#include "snow.h"

#include <errno.h>
#include <string.h>

typedef struct {
    uint16_t slot_count;
    uint16_t data_start;
    uint8_t type;
} header_t;

#define page_header(page) ((header_t*)((page).data))

#define page_ptrs(page) ((uint16_t*)((page).data + sizeof(header_t)))

result_t
page_init(const page_t page, const size_t size, const uint8_t type) {
    ensure(size <= UINT16_MAX);

    header_t* header = page_header(page);
    header->slot_count = 0;
    header->data_start = (uint16_t)size;
    header->type = type;

    return SUCCESS;
}

static uint16_t
page_free_space(const page_t page) {
    const header_t* header = page_header(page);
    return header->data_start - sizeof(header_t) - sizeof(uint16_t) * header->slot_count;
}

result_t
page_insert(const page_t page, const uint16_t index, const byte_t* data, const size_t size) {
    header_t* header = page_header(page);
    ensure(index <= header->slot_count);

    const uint16_t free_space = page_free_space(page);
    if (free_space < size) {
        failure(ENOSPC, "not enough free space: %d, size: %d", free_space, size);
    }

    header->slot_count += 1;
    header->data_start -= size;

    ensure(data != NULL);
    memcpy(page.data + header->data_start, data, size);

    uint16_t* ptrs = page_ptrs(page);

    const uint16_t trailing = header->slot_count - index;
    if (trailing > 0) {
        memmove(ptrs + index + 1, ptrs + index, sizeof(uint16_t) * trailing);
    }

    ptrs[index] = header->data_start;

    return SUCCESS;
}

result_t
page_lookup(const page_t page, const uint16_t index, byte_t** out) {
    const header_t* header = page_header(page);
    ensure(index < header->slot_count);

    ensure(out != NULL);
    *out = page.data + page_ptrs(page)[index];

    return SUCCESS;
}

describe(page) {
    byte_t data[512];
    const page_t page = { .data = data, .id = 3 };

    before_each() {
        asserteq_int(page_init(page, sizeof(data), 0), SUCCESS);
    }

    after_each() {
        error_clear();
    }

    it("insert a string") {
        const byte_t value[] = "hello world";
        asserteq_int(page_insert(page, 0, value, sizeof(value)), SUCCESS);
    }

    it("lookup a string") {
        const byte_t value[] = "a string";
        asserteq_int(page_insert(page, 0, value, sizeof(value)), SUCCESS);

        byte_t* out;
        asserteq_int(page_lookup(page, 0, &out), SUCCESS);

        asserteq_str((char*) out, (char*) value);
    }

    it("insert fails if not enough space") {
        const byte_t value[] = "a string";
        asserteq_int(page_insert(page, 0, value, sizeof(data)), FAILURE);
        asserteq_int(error_get_code(), ENOSPC);
    }

    it("lookup fails if index is out of bounds") {
        asserteq_int(page_lookup(page, 0, NULL), FAILURE);
        asserteq_int(error_get_code(), EINVAL);
    }
}
