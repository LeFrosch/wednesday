#include <stdatomic.h>
#include <stdbool.h>

#include "deffer.h"
#include "error.h"
#include "latch.h"
#include "pager.h"
#include "util.h"
#include "winter.h"

enum {
    PAGE_FLAG_DIRTY = (1u << 1),
    PAGE_FLAG_REF = (1u << 2),
    PAGE_FLAG_EXCLUSIVE = (1u << 3),
};

typedef uint32_t ring_id_t;

typedef struct {
    page_id_t page_id;
    ring_id_t ring_id;
} page_t;

typedef struct {
    _Atomic page_id_t id;
    _Atomic uint8_t flags;

    latch_t latch;

    unsigned char* data;
} ring_entry_t;

typedef struct bucket {
    page_t page;
    struct bucket* next;
} bucket_t;

typedef struct {
    latch_t latch;
    page_t first;
    page_t second;
    bucket_t* overflow;
} dict_entry_t;

struct pager_t {
    uint16_t page_size;
    uint32_t size;
    uint32_t mask;

    ring_entry_t* ring;
    dict_entry_t* dict;

    _Atomic ring_id_t ring_alloc_head;
    _Atomic ring_id_t ring_evict_head;
    _Atomic uint32_t page_count;
};

result_t
pager_open(pager_t* pager, const uint16_t page_size, const uint32_t directory_size) {
    ensure((directory_size & (directory_size - 1)) == 0);

    pager->page_size = page_size;
    pager->size = (uint32_t)(directory_size * 0.7);
    pager->page_count = 0;
    pager->mask = directory_size - 1;
    pager->ring_alloc_head = 0;
    pager->ring_evict_head = 0;

    try_alloc(pager->dict, sizeof(dict_entry_t) * directory_size);
    errdefer(free, pager->dict);

    try_alloc(pager->ring, sizeof(ring_entry_t) * pager->size);
    errdefer(free, pager->ring);

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

static bool
entry_dict_get(const dict_entry_t* entry, const page_id_t id, page_t* out) {
    assert_latch_read_access(entry->latch);

    if (entry->first.page_id == id) {
        *out = entry->first;
        return true;
    }
    if (entry->second.page_id == id) {
        *out = entry->second;
        return true;
    }

    for (const bucket_t* bucket = entry->overflow; bucket != nullptr; bucket = bucket->next) {
        if (bucket->page.page_id == id) {
            *out = bucket->page;
            return true;
        }
    }

    return false;
}

static result_t
entry_dict_put(dict_entry_t* entry, page_t page) {
    assert_latch_write_access(entry->latch);

    if (entry->first.page_id == 0) {
        entry->first = page;
        return SUCCESS;
    }
    if (entry->second.page_id == 0) {
        entry->second = page;
        return SUCCESS;
    }

    if (entry->overflow == nullptr) {
        try_alloc(entry->overflow, sizeof(bucket_t));
    }

    bucket_t* bucket = entry->overflow;
    while (true) {
        if (bucket->page.page_id == 0) {
            bucket->page = page;
            return SUCCESS;
        }

        if (bucket->next == nullptr) {
            try_alloc(bucket->next, sizeof(bucket_t));
        }

        bucket = bucket->next;
    }
}

static result_t
pager_new_page(pager_t* pager, dict_entry_t* dict_entry, const page_id_t id, ring_entry_t** out) {
    assert_latch_write_access(dict_entry->latch);

    ring_id_t ring_id;
    ring_entry_t* ring_entry;
    while (true) {
        ring_id = atomic_fetch_add(&pager->ring_alloc_head, 1) % pager->size;
        ring_entry = &pager->ring[ring_id];

        page_id_t expected = 0;
        if (atomic_compare_exchange_weak(&ring_entry->id, &expected, id)) {
            break;
        }
    }

    ring_entry->latch = 0;
    ring_entry->flags = 0;

    // TODO: errdefer atomic_store(&ring_entry->id, 0) ?

    if (ring_entry->data == nullptr) {
        ring_entry->data = malloc(pager->page_size);

        if (ring_entry->data == nullptr) {
            atomic_store(&ring_entry->id, 0);
            failure(ENOMEM, "failed to allocate page");
        }
    }

    // store the ring id in the begining of the page
    memcpy(ring_entry->data, &ring_id, sizeof(ring_id_t));

    const page_t page = { id, ring_id };

    handle(entry_dict_put(dict_entry, page)) {
        atomic_store(&ring_entry->id, 0);
        forward();
    }

    *out = ring_entry;
    return SUCCESS;
}

result_t
pager_evict_page(pager_t* pager) {
    uint32_t i = atomic_load(&pager->ring_evict_head);
    const uint32_t end = i + pager->size * 2;

    while (i < end) {
        i = atomic_fetch_add(&pager->ring_evict_head, 1);

        ring_entry_t* ring_entry = &pager->ring[i % pager->size];
        dict_entry_t* dict_entry = &pager->dict[pager_hash(pager, ring_entry->id)];

        latch_acquire_write(&dict_entry->latch);
        defer(latch_release_write, dict_entry->latch);

        if (!latch_available(&ring_entry->latch)) {
            continue;
        }

        const uint8_t flags = atomic_load(&ring_entry->flags);
        if (flags & PAGE_FLAG_REF) {
            atomic_fetch_and(&ring_entry->flags, ~PAGE_FLAG_REF);
            continue;
        }

        atomic_store(&ring_entry->id, 0);
        atomic_store(&ring_entry->flags, 0);
    }

    failure(ENOMEM, "no pages to evict");
}

result_t
pager_fix(pager_t* pager, const page_id_t id, const bool exclusive, unsigned char** out) {
    ensure(pager != nullptr);
    ensure(out != nullptr);

    if (id == 0) {
        failure(EINVAL, "invalid page id");
    }

    ring_entry_t* ring_entry;

    { // lock the hash table entry for this scope
        dict_entry_t* dict_entry = &pager->dict[pager_hash(pager, id)];
        latch_acquire(&dict_entry->latch, true);
        defer(latch_release_write, dict_entry->latch);

        page_t page;
        if (entry_dict_get(dict_entry, id, &page)) {
            ring_entry = &pager->ring[page.ring_id];
        } else {
            uint32_t count = atomic_load(&pager->page_count);
            while (true) {
                if (count >= pager->size) {
                    try(pager_evict_page(pager));
                    break;
                }

                if (atomic_compare_exchange_weak(&pager->page_count, &count, count + 1)) {
                    break;
                }
            }

            handle(pager_new_page(pager, dict_entry, id, &ring_entry)) {
                atomic_fetch_sub(&pager->page_count, 1);
                forward();
            }
        }
    }

    latch_acquire(&ring_entry->latch, exclusive);

    if (exclusive) {
        atomic_fetch_or(&ring_entry->flags, PAGE_FLAG_EXCLUSIVE);
    }

    *out = ring_entry->data;

    return SUCCESS;
}

static ring_entry_t*
pager_get_ring_entry(const pager_t* pager, const unsigned char* page) {
    ring_id_t ring_id;
    memcpy(&ring_id, page, sizeof(ring_id_t));

    return &pager->ring[ring_id];
}

void
pager_unfix(const pager_t* pager, const unsigned char* page) {
    ring_entry_t* entry = pager_get_ring_entry(pager, page);

    const uint8_t flags = atomic_load_explicit(&entry->flags, memory_order_relaxed);
    if (flags & PAGE_FLAG_EXCLUSIVE) {
        // latch guarantees exclusive access, safe to do non-CAS write
        atomic_store_explicit(&entry->flags, PAGE_FLAG_DIRTY | PAGE_FLAG_REF, memory_order_relaxed);
        latch_release_write(&entry->latch);
    } else {
        // concurrent writes possible
        atomic_fetch_or_explicit(&entry->flags, PAGE_FLAG_REF, memory_order_seq_cst);
        latch_release_read(&entry->latch);
    }
}

result_t
pager_close(pager_t* pager) {
    ensure(pager != nullptr);

    for (uint32_t i = 0; i < pager->size; ++i) {
        const ring_entry_t* entry = &pager->ring[i];
        if (entry->data != nullptr) {
            free(entry->data);
        }
    }
    free(pager->ring);

    for (uint32_t i = 0; i <= pager->mask; ++i) {
        bucket_t* bucket = pager->dict[i].overflow;
        while (bucket != nullptr) {
            bucket_t* next = bucket->next;
            free(bucket);
            bucket = next;
        }
    }
    free(pager->dict);

    *pager = (pager_t){ 0 };

    return SUCCESS;
}

describe(pager) {
    static pager_t pager;

    before_each() {
        assert_success(pager_open(&pager, 124, 64));
    }

    after_each() {
        assert_success(pager_close(&pager));
        error_clear();
    }

    it("fix a page") {
        unsigned char* page;
        assert_success(pager_fix(&pager, 3, false, &page));
    }

    it("fix the same page twice") {
        unsigned char* first;
        assert_success(pager_fix(&pager, 3, false, &first));
        unsigned char* second;
        assert_success(pager_fix(&pager, 3, false, &second));

        asserteq_ptr(first, second);
    }

    it("fix two different pages") {
        unsigned char* first;
        assert_success(pager_fix(&pager, 3, false, &first));
        unsigned char* second;
        assert_success(pager_fix(&pager, 4, false, &second));

        assertneq_ptr(first, second);
    }

    it("acquire the page latch") {
        unsigned char* page;
        assert_success(pager_fix(&pager, 3, false, &page));
        asserteq_int(pager_get_ring_entry(&pager, page)->latch, 1);

        assert_success(pager_fix(&pager, 4, true, &page));
        asserteq_int(pager_get_ring_entry(&pager, page)->latch, -1);
    }

    it("fill pager") {
        for (uint32_t i = 1; i <= pager.size; ++i) {
            unsigned char* page;
            assert_success(pager_fix(&pager, i, false, &page));
        }

        unsigned char* page;
        assert_failure(pager_fix(&pager, pager.size + 1, false, &page), ENOMEM);
    }

    parallel("fill pager", 8) {
        for (uint32_t i = 1; i <= pager.size; ++i) {
            unsigned char* page;
            assert_success(pager_fix(&pager, i, false, &page));
        }

        unsigned char* page;
        assert_failure(pager_fix(&pager, pager.size + 1, false, &page), ENOMEM);
    }

    it("fix invalid page id") {
        unsigned char* page;
        assert_failure(pager_fix(&pager, 0, true, &page), EINVAL);
    }

    it("release the page latch") {
        unsigned char* page;
        assert_success(pager_fix(&pager, 3, false, &page));
        asserteq_int(pager_get_ring_entry(&pager, page)->latch, 1);

        pager_unfix(&pager, page);
        asserteq_int(pager_get_ring_entry(&pager, page)->latch, 0);

        assert_success(pager_fix(&pager, 3, true, &page));
        asserteq_int(pager_get_ring_entry(&pager, page)->latch, -1);

        pager_unfix(&pager, page);
        asserteq_int(pager_get_ring_entry(&pager, page)->latch, 0);
    }

    parallel("fix and unfix pages exclusive", 8) {
        for (uint32_t i = 1; i <= pager.size; ++i) {
            unsigned char* page;
            assert_success(pager_fix(&pager, i, true, &page));
            pager_unfix(&pager, page);
        }
    }
}
