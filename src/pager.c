#include <stdatomic.h>
#include <stdbool.h>

#include "deffer.h"
#include "error.h"
#include "latch.h"
#include "pager.h"
#include "snow.h"
#include "util.h"

#define ENTRY_ELEMENT_COUNT 4

enum {
    PAGE_DIRTY = (1u << 1),
    PAGE_REF = (1u << 2),
    PAGE_EXCLUSIVE = (1u << 3),
};

/// Meta-information stored tracked for every page. Stored in front of the
/// actual page data.
typedef struct {
    // linked list on hashtable overflow
    page_t overflow;

    latch_t latch;
    page_id_t id;

    _Atomic uint8_t flags;
} header_t;

/// Entry in the hash table directory. Overflows are stored in a linked list.
typedef struct {
    latch_t latch;
    page_t page;
} entry_t;

struct pager_t {
    uint16_t page_size;
    uint32_t max_pages;
    uint32_t mask;

    entry_t* directory;
    header_t** ring;

    _Atomic uint32_t ring_head;
    _Atomic uint32_t page_count;
};

result_t
pager_open(pager_t* pager, const uint16_t page_size, const uint32_t directory_size) {
    ensure((directory_size & (directory_size - 1)) == 0);

    pager->page_size = page_size;
    pager->max_pages = (uint32_t)(directory_size * 0.7);
    pager->page_count = 0;
    pager->mask = directory_size - 1;
    pager->ring_head = 0;

    try_alloc(pager->directory, sizeof(entry_t) * directory_size);
    errdefer(free, pager->directory);

    try_alloc(pager->ring, sizeof(header_t*) * pager->max_pages);
    errdefer(free, pager->ring);

    return SUCCESS;
}

/// Transforms the pointer to the page data to a corresponding pointer to the
/// header.
static header_t*
page_get_header(const page_t page) {
    return (header_t*)((page) - sizeof(header_t));
}

/// Transforms the pointer to the page header to the corresponding pointer to the
/// page data.
static page_t
header_get_page(header_t* header) {
    return (page_t)(header + 1);
}

static uint32_t
pager_hash(const pager_t* pager, const page_id_t id) {
    uint32_t hash = id;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash);

    return hash & pager->mask;
}

static page_t
entry_get_page(const entry_t* entry, const page_id_t id) {
    assert_latch_read_access(entry->latch);

    page_t page = entry->page;
    while (page != NULL) {
        const header_t* header = page_get_header(page);
        if (header->id == id) {
            break;
        }

        page = header->overflow;
    }

    return page;
}

static result_t
pager_new_page(pager_t* pager, entry_t* entry, const page_id_t id, page_t* out) {
    assert_latch_write_access(entry->latch);

    header_t* header;
    try_alloc(header, sizeof(header_t) + pager->page_size);
    header->id = id;
    header->overflow = entry->page;

    while (1) {
        const uint32_t i = atomic_fetch_add_explicit(&pager->ring_head, 1, memory_order_seq_cst) % pager->max_pages;
        if (pager->ring[i] == NULL) {
            pager->ring[i] = header;
            break;
        }
    }

    const page_t page = header_get_page(header);
    entry->page = page;
    *out = page;

    return SUCCESS;
}

result_t
pager_fix(pager_t* pager, const page_id_t id, const bool exclusive, page_t* out) {
    ensure(pager != NULL);
    ensure(out != NULL);

    if (id == 0) {
        failure(EINVAL, "invalid page id");
    }

    page_t page = NULL;

    { // lock the hash table entry for this scope
        entry_t* entry = &pager->directory[pager_hash(pager, id)];
        latch_acquire(&entry->latch, true);
        defer(latch_release_write, entry->latch);

        page = entry_get_page(entry, id);

        if (page == NULL) {
            uint32_t count = atomic_load_explicit(&pager->page_count, memory_order_relaxed);
            while (1) {
                if (count >= pager->max_pages) {
                    // TODO: implement eviction
                    failure(ENOMEM, "pager full");
                }

                const bool success = atomic_compare_exchange_weak_explicit(
                  &pager->page_count, &count, count + 1, memory_order_seq_cst, memory_order_relaxed
                );

                if (success) {
                    break;
                }
            }

            handle(pager_new_page(pager, entry, id, &page)) {
                atomic_fetch_sub_explicit(&pager->page_count, 1, memory_order_seq_cst);
                forward();
            }
        }
    }

    header_t* header = page_get_header(page);
    latch_acquire(&header->latch, exclusive);

    if (exclusive) {
        // relaxed is fine, memory order guaranteed by latch
        atomic_fetch_or_explicit(&header->flags, PAGE_EXCLUSIVE, memory_order_relaxed);
    }

    *out = page;
    return SUCCESS;
}

void
pager_unfix(const page_t page) {
    header_t* header = page_get_header(page);

    const uint8_t flags = atomic_load_explicit(&header->flags, memory_order_relaxed);
    if (flags & PAGE_EXCLUSIVE) {
        // latch guarantees exclusive access, safe to do non-CAS write
        atomic_store_explicit(&header->flags, PAGE_DIRTY | PAGE_REF, memory_order_relaxed);
        latch_release_write(&header->latch);
    } else {
        // concurrent writes possible
        atomic_fetch_or_explicit(&header->flags, PAGE_REF, memory_order_relaxed);
        latch_release_read(&header->latch);
    }
}

result_t
pager_close(pager_t* pager) {
    ensure(pager != NULL);

    for (uint32_t i = 0; i < pager->max_pages; ++i) {
        header_t* header = pager->ring[i];
        if (header != NULL) {
            free(header);
        }
    }

    free(pager->directory);
    free(pager->ring);

    *pager = (pager_t){ 0 };

    return SUCCESS;
}

describe(pager) {
    pager_t pager;

    before_each() {
        assert_success(pager_open(&pager, 124, 64));
    }

    after_each() {
        assert_success(pager_close(&pager));
        error_clear();
    }

    it("fix a page") {
        page_t page;
        assert_success(pager_fix(&pager, 3, false, &page));
    }

    it("fix the same page twice") {
        page_t first;
        assert_success(pager_fix(&pager, 3, false, &first));
        page_t second;
        assert_success(pager_fix(&pager, 3, false, &second));

        asserteq_ptr(first, second);
    }

    it("fix two different pages") {
        page_t first;
        assert_success(pager_fix(&pager, 3, false, &first));
        page_t second;
        assert_success(pager_fix(&pager, 4, false, &second));

        assertneq_ptr(first, second);
    }

    it("acquire the page latch") {
        page_t page;
        assert_success(pager_fix(&pager, 3, false, &page));
        asserteq_int(page_get_header(page)->latch, 1);

        assert_success(pager_fix(&pager, 4, true, &page));
        asserteq_int(page_get_header(page)->latch, -1);
    }

    it("fill pager") {
        for (uint32_t i = 1; i <= pager.max_pages; ++i) {
            page_t page;
            assert_success(pager_fix(&pager, i, false, &page));
        }

        page_t page;
        assert_failure(pager_fix(&pager, pager.max_pages + 1, false, &page), ENOMEM);
    }

    it("fix invalid page id") {
        page_t page;
        assert_failure(pager_fix(&pager, 0, true, &page), EINVAL);
    }

    it("release the page latch") {
        page_t page;
        assert_success(pager_fix(&pager, 3, false, &page));
        asserteq_int(page_get_header(page)->latch, 1);

        pager_unfix(page);
        asserteq_int(page_get_header(page)->latch, 0);

        assert_success(pager_fix(&pager, 3, true, &page));
        asserteq_int(page_get_header(page)->latch, -1);

        pager_unfix(page);
        asserteq_int(page_get_header(page)->latch, 0);
    }
}
