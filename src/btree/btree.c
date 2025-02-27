#include <string.h>

#include "btree.h"
#include "pager/pager.h"

// The type of page. First byte of any page.
#define PT_STORE_I 1 /* inner store page: no data, keys only */
#define PT_STORE_L 2 /* leaf store page: data */

/**
 * Stores information tracked for every in memory page. Stored inside the extra
 * data provided by the pager.
 */
typedef struct {
    uint8_t page_type;   /* the type of the page */
    uint16_t cell_count; /* the number of cells stored on the page */
    uint16_t cell_start; /* index into the page where the cell data starts, 0 if there are no cells */
} data_t;

/**
 * The size of a single page. TODO: make this configurable
 */
#define PAGE_SIZE 1024

result_t
btree_open(pager_t** out, const uint32_t page_cache_size) {
    return pager_open(out, PAGE_SIZE, page_cache_size, 64);
}

static void
writeUint8(const uint8_t value, unsigned char* buf, const size_t offset) {
    buf[offset] = value;
}

static void
writeUint16(const uint16_t value, unsigned char* buf, const size_t offset) {
    buf[offset + 1] = (value >> 8) & 0xff;
    buf[offset] = value & 0xff;
}

/**
 * Writes all data fields that are stored on the page to the page.
 */
static void
btree_data_write(const data_t* data, unsigned char* page) {
    writeUint8(data->page_type, page, 0);
    writeUint16(data->cell_count, page, 1);
    writeUint16(data->cell_start, page, 3);
}

result_t
btree_store_create(pager_t* pager, pgno_t* out) {
    ensure(pager);
    ensure_no_errors();

    const uint32_t page_count = pager_get_page_count(pager);

    // create a new page at the end of the file
    unsigned char* page;
    ensure(pager_get(pager, page_count, &page));

    // update the page data
    data_t* data = pager_get_extra_data(page);
    data->page_type = PT_STORE_L;
    data->cell_count = 0;
    data->cell_start = 0;

    btree_data_write(data, page);

    *out = page_count;
    return SUCCESS;
}
