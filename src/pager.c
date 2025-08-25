#include <stdatomic.h>
#include <stdbool.h>

#include "deffer.h"
#include "error.h"
#include "latch.h"
#include "pager.h"
#include "util.h"
#include "winter.h"

enum {
    PAGE_FLAG_DIRTY = (1u << 0),
    PAGE_FLAG_REF = (1u << 1),
    PAGE_FLAG_EXCLUSIVE = (1u << 2),
};

typedef struct {
    page_id_t id;
    latch_t latch;

    uint32_t ring_index;

    _Atomic uint8_t flags;
} header_t;

typedef struct {
    page_id_t page_id;
    header_t* header;
} page_t;

typedef struct bucket {
    page_t value;
    struct bucket* next;
} bucket_t;

typedef struct {
    latch_t latch;
    page_t first;
    page_t second;
    bucket_t* overflow;
} hash_entry_t;

typedef struct {
    _Atomic page_id_t page_id;
    header_t* header;
} ring_entry_t;

struct pager_t {
    uint16_t page_size;
    uint32_t size;
    uint32_t mask;

    hash_entry_t* directory;
    ring_entry_t* ring;

    _Atomic uint32_t page_count;
    _Atomic uint32_t evict_head;
    _Atomic uint32_t create_head;
};

static unsigned char*
header_get_data(const header_t* page) {
    return (unsigned char*)(page + 1);
}

static header_t*
header_from_data(const unsigned char* data) {
    return (header_t*)data - 1;
}

result_t
pager_open(pager_t* pager, const uint16_t page_size, const uint32_t directory_size) {
    ensure((directory_size & (directory_size - 1)) == 0);

    pager->page_size = page_size;
    pager->size = (uint32_t)(directory_size * 0.7);
    pager->page_count = 0;
    pager->mask = directory_size - 1;
    pager->evict_head = 0;
    pager->create_head = 0;

    try_alloc(pager->directory, sizeof(hash_entry_t) * directory_size);
    errdefer(free, pager->directory);

    try_alloc(pager->ring, sizeof(ring_entry_t) * pager->size);
    errdefer(free, pager->ring);

    for (uint32_t i = 0; i < directory_size; ++i) {
        latch_init(&pager->directory[i].latch);
    }

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
entry_directory_lookup(const hash_entry_t* hash_entry, const page_id_t id, header_t** out) {
    assert_latch_read_access(hash_entry->latch);

    if (hash_entry->first.page_id == id) {
        *out = hash_entry->first.header;
        return true;
    }
    if (hash_entry->second.page_id == id) {
        *out = hash_entry->second.header;
        return true;
    }

    for (const bucket_t* bucket = hash_entry->overflow; bucket != nullptr; bucket = bucket->next) {
        if (bucket->value.page_id == id) {
            *out = bucket->value.header;
            return true;
        }
    }

    return false;
}

static result_t
pager_directory_insert(hash_entry_t* hash_entry, header_t* header) {
    assert_latch_write_access(hash_entry->latch);

    if (hash_entry->first.page_id == 0) {
        hash_entry->first.page_id = header->id;
        hash_entry->first.header = header;
        return SUCCESS;
    }
    if (hash_entry->second.page_id == 0) {
        hash_entry->second.page_id = header->id;
        hash_entry->second.header = header;
        return SUCCESS;
    }

    if (hash_entry->overflow == nullptr) {
        try_alloc(hash_entry->overflow, sizeof(bucket_t));
    }

    bucket_t* bucket = hash_entry->overflow;
    while (true) {
        if (bucket->value.page_id == 0) {
            bucket->value.page_id = header->id;
            bucket->value.header = header;
            return SUCCESS;
        }

        if (bucket->next == nullptr) {
            try_alloc(bucket->next, sizeof(bucket_t));
        }

        bucket = bucket->next;
    }
}

static void
pager_directory_remove(hash_entry_t* hash_entry, const page_id_t id) {
    assert_latch_write_access(hash_entry->latch);

    if (hash_entry->first.page_id == id) {
        hash_entry->first.page_id = 0;
        return;
    }
    if (hash_entry->second.page_id == id) {
        hash_entry->second.page_id = 0;
        return;
    }

    for (bucket_t* bucket = hash_entry->overflow; bucket != nullptr; bucket = bucket->next) {
        if (bucket->value.page_id == id) {
            bucket->value.page_id = 0;
            break;
        }
    }
}

static result_t
pager_create(pager_t* pager, hash_entry_t* hash_entry, const page_id_t id, header_t** out) {
    assert_latch_write_access(hash_entry->latch);

    while (true) {
        const uint32_t index = atomic_fetch_add(&pager->create_head, 1) % pager->size;

        ring_entry_t* ring_entry = &pager->ring[index];
        page_id_t expected = 0;
        if (!atomic_compare_exchange_weak(&ring_entry->page_id, &expected, id)) {
            continue;
        }

        if (ring_entry->header == nullptr) {
            try_alloc(ring_entry->header, sizeof(header_t) + pager->page_size);
        }

        ring_entry->header->ring_index = index;
        ring_entry->header->id = id;
        atomic_store(&ring_entry->header->flags, 0);
        latch_init(&ring_entry->header->latch);

        handle(pager_directory_insert(hash_entry, ring_entry->header)) {
            atomic_store(&ring_entry->page_id, 0);
            forward();
        }

        *out = ring_entry->header;

        return SUCCESS;
    }
}

bool
pager_directory_find_entry(const pager_t* pager, ring_entry_t* ring_entry, hash_entry_t** out) {
    page_id_t page_id = atomic_load(&ring_entry->page_id);

    while (page_id != 0) {
        hash_entry_t* hash_entry = &pager->directory[pager_hash(pager, page_id)];
        latch_acquire_write(&hash_entry->latch);

        if (atomic_compare_exchange_strong(&ring_entry->page_id, &page_id, page_id)) {
            *out = hash_entry;
            return true;
        }

        latch_release_write(&hash_entry->latch);
    }

    return false;
}

result_t
pager_evict(pager_t* pager) {
    for (uint32_t i = 0; i < pager->size * 2; ++i) {
        const uint32_t index = atomic_fetch_add(&pager->evict_head, 1) % pager->size;
        ring_entry_t* ring_entry = &pager->ring[index];

        hash_entry_t* entry;
        if (!pager_directory_find_entry(pager, ring_entry, &entry)) {
            continue;
        }
        defer(latch_release_write, entry->latch);

        if (!latch_available(&ring_entry->header->latch)) {
            continue;
        }

        const uint8_t flags = atomic_load(&ring_entry->header->flags);
        if (flags & PAGE_FLAG_REF) {
            atomic_fetch_and(&ring_entry->header->flags, ~PAGE_FLAG_REF);
            continue;
        }

        pager_directory_remove(entry, ring_entry->page_id);
        atomic_store(&ring_entry->page_id, 0);

        return SUCCESS;
    }

    failure(ENOMEM, msg("no pages to evict"));
}

bool
pager_lookup(const hash_entry_t* hash_entry, const page_id_t id, const bool exclusive, unsigned char** out) {
    header_t* header;
    if (!entry_directory_lookup(hash_entry, id, &header)) {
        return false;
    }

    latch_acquire(&header->latch, exclusive);

    if (exclusive) {
        // only needs to be visible to this thread, latch was acquired exclusively
        atomic_fetch_or_explicit(&header->flags, PAGE_FLAG_EXCLUSIVE, memory_order_relaxed);
    }

    *out = header_get_data(header);

    return true;
}

result_t
pager_fix(pager_t* pager, const page_id_t id, const bool exclusive, unsigned char** out) {
    ensure(pager != nullptr);
    ensure(out != nullptr);

    if (id == 0) {
        failure(EINVAL, msg("invalid page id"));
    }

    hash_entry_t* hash_entry = &pager->directory[pager_hash(pager, id)];
    { // fast pass, try to look up the page with read-only lock
        latch_acquire_read(&hash_entry->latch);
        defer(latch_release_read, hash_entry->latch);

        if (pager_lookup(hash_entry, id, exclusive, out)) {
            return SUCCESS;
        }
    }

    // ensure that there is at least enough capacity to allocate a new page if required
    uint32_t count = atomic_load(&pager->page_count);
    while (true) {
        if (count >= pager->size - 1) {
            try(pager_evict(pager));
            break;
        }

        if (atomic_compare_exchange_weak(&pager->page_count, &count, count + 1)) {
            break;
        }
    }

    latch_acquire_write(&hash_entry->latch);
    defer(latch_release_write, hash_entry->latch);

    // retry the lookup after acquiring the write lock
    if (pager_lookup(hash_entry, id, exclusive, out)) {
        atomic_fetch_sub(&pager->page_count, 1);
        return SUCCESS;
    }

    header_t* header;
    handle(pager_create(pager, hash_entry, id, &header)) {
        atomic_fetch_sub(&pager->page_count, 1);
        forward();
    }

    latch_acquire(&header->latch, exclusive);

    if (exclusive) {
        // only needs to be visible to this thread, latch was acquired exclusively
        atomic_fetch_or_explicit(&header->flags, PAGE_FLAG_EXCLUSIVE, memory_order_relaxed);
    }

    *out = header_get_data(header);

    return SUCCESS;
}

void
pager_unfix(const unsigned char* data) {
    header_t* header = header_from_data(data);

    const uint8_t flags = atomic_load_explicit(&header->flags, memory_order_acquire);
    if (flags & PAGE_FLAG_EXCLUSIVE) {
        // latch guarantees exclusive access, safe to do non-CAS write
        atomic_store_explicit(&header->flags, PAGE_FLAG_DIRTY | PAGE_FLAG_REF, memory_order_relaxed);
        latch_release_write(&header->latch);
    } else {
        // concurrent writes possible
        atomic_fetch_or_explicit(&header->flags, PAGE_FLAG_REF, memory_order_seq_cst);
        latch_release_read(&header->latch);
    }
}

result_t
pager_close(pager_t* pager) {
    ensure(pager != nullptr);

    for (uint32_t i = 0; i < pager->size; ++i) {
        const ring_entry_t* ring_entry = &pager->ring[i];
        if (ring_entry->header != nullptr) {
            free(ring_entry->header);
        }
    }
    free(pager->ring);

    for (uint32_t i = 0; i <= pager->mask; ++i) {
        bucket_t* bucket = pager->directory[i].overflow;
        while (bucket != nullptr) {
            bucket_t* next = bucket->next;
            free(bucket);
            bucket = next;
        }
    }
    free(pager->directory);

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
        asserteq_int(header_from_data(page)->latch, 1);

        assert_success(pager_fix(&pager, 4, true, &page));
        asserteq_int(header_from_data(page)->latch, -1);
    }

    it("fill pager") {
        for (uint32_t i = 1; i <= pager.size - 1; ++i) {
            unsigned char* page;
            assert_success(pager_fix(&pager, i, false, &page));
        }

        unsigned char* page;
        assert_failure(pager_fix(&pager, pager.size, false, &page), ENOMEM);
    }

    parallel("fill pager", 8) {
        for (uint32_t i = 1; i <= pager.size - 8; ++i) {
            unsigned char* page;
            assert_success(pager_fix(&pager, i, false, &page));
        }
    }

    it("fix invalid page id") {
        unsigned char* page;
        assert_failure(pager_fix(&pager, 0, true, &page), EINVAL);
    }

    it("release the page latch") {
        unsigned char* page;
        assert_success(pager_fix(&pager, 3, false, &page));
        asserteq_int(header_from_data(page)->latch, 1);

        pager_unfix(page);
        asserteq_int(header_from_data(page)->latch, 0);

        assert_success(pager_fix(&pager, 3, true, &page));
        asserteq_int(header_from_data(page)->latch, -1);

        pager_unfix(page);
        asserteq_int(header_from_data(page)->latch, 0);
    }

    parallel("fix and unfix pages non-exclusive", 8) {
        for (uint32_t i = 1; i <= pager.size * 10; ++i) {
            unsigned char* page;
            assert_success(pager_fix(&pager, i, false, &page));
            assertis(header_from_data(page)->latch > 0);
            pager_unfix(page);
        }
    }

    parallel("fix and unfix pages exclusive", 8) {
        for (uint32_t i = 1; i <= pager.size * 10; ++i) {
            unsigned char* page;
            assert_success(pager_fix(&pager, i, true, &page));
            assertis(header_from_data(page)->latch == -1);
            pager_unfix(page);
        }
    }
}
