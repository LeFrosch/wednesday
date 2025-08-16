#include "pager.h"
#include "error.h"
#include "latch.h"
#include "snow.h"

#include <stdbool.h>

#define ENTRY_ELEMENT_COUNT 4

typedef struct {
    latch_t latch;
    page_id_t ids[ENTRY_ELEMENT_COUNT];
    page_t pages[ENTRY_ELEMENT_COUNT];
} __attribute__((aligned(64))) entry_t;

struct pager_t {
    uint16_t page_size;
    uint32_t max_pages;
    uint32_t mask;

    uint32_t page_count;

    entry_t* directory;
};

typedef struct {
    latch_t latch;
    page_id_t id;
    bool dirty;
} header_t;

result_t
pager_open(pager_t* pager, const uint16_t page_size, const uint32_t max_pages) {
    ensure((max_pages & (max_pages - 1)) == 0);

    pager->page_size = page_size;
    pager->max_pages = max_pages;
    pager->page_count = 0;
    pager->mask = max_pages - 1;

    pager->directory = aligned_alloc(64, sizeof(entry_t) * max_pages);
    if (!pager->directory) {
        failure(ENOMEM, "could not allocate pager directory");
    }
    memset(pager->directory, 0, sizeof(entry_t) * max_pages);

    return SUCCESS;
}

static uint32_t
pager_hash(const pager_t* pager, const page_id_t id) {
    uint32_t hash = id;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash);

    return hash & pager->mask;
}

static int
entry_free_slot(const entry_t* entry) {
    for (int i = 0; i < ENTRY_ELEMENT_COUNT; i++) {
        if (entry->ids[i] == 0) {
            return i;
        }
    }

    return -1;
}

result_t
pager_fix(pager_t* pager, const page_id_t id, const bool exclusive, page_t* out) {
    ensure(pager != NULL);
    ensure(out != NULL);

    if (id == 0) {
        failure(EINVAL, "invalid page id");
    }

    // TODO: implement latching
    (void)exclusive;

    entry_t* entry = &pager->directory[pager_hash(pager, id)];
    for (int i = 0; i < ENTRY_ELEMENT_COUNT; i++) {
        if (entry->ids[i] == id) {
            *out = entry->pages[i];
            return SUCCESS;
        }
    }

    if (pager->page_count >= pager->max_pages) {
        // TODO: implement eviction
        failure(ENOMEM, "pager full");
    }

    const int slot = entry_free_slot(entry);
    if (slot < 0) {
        failure(ENOMEM, "pager conflict");
    }

    pager->page_count += 1;

    unsigned char* page = malloc(sizeof(page_t) + sizeof(header_t));
    page += sizeof(header_t); // adjust pointer, header is stored above the data

    entry->ids[slot] = id;
    entry->pages[slot] = page;

    *out = page;

    return SUCCESS;
}

result_t
pager_close(pager_t* pager) {
    ensure(pager != NULL);

    // invariant: directory size == max pages, if this is a good idea, idk?
    for (uint32_t i = 0; i < pager->max_pages; i++) {
        const entry_t entry = pager->directory[i];
        for (int k = 0; k < ENTRY_ELEMENT_COUNT; k++) {
            if (entry.ids[k] != 0) {
                // adjust the pointer to get the original pointer returned by malloc
                free(entry.pages[k] - sizeof(header_t));
            }
        }
    }
    free(pager->directory);

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

    it("directory entry size") {
        asserteq_int(sizeof(entry_t), 64);
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
}
