#include <stdatomic.h>
#include <stdbool.h>

#include "deffer.h"
#include "error.h"
#include "latch.h"
#include "pager.h"
#include "util.h"
#include "winter.h"

enum {
    /// If the page content was modified.
    PAGE_FLAG_DIRTY = (1u << 0),

    /// Second chance bit. Set if the page is fixed and provides a second chance
    /// for the page on eviction.
    PAGE_FLAG_REF = (1u << 1),

    /// Set if the page was fixed exclusively (i.e. with write lock).
    PAGE_FLAG_EXCLUSIVE = (1u << 2),
};

/// Page header stored before of the actual page data in memory and is not
/// persisted to disk.
typedef struct {
    page_id_t id;

    /// Read-write lock of the page only protects the page's content.
    latch_t latch;

    /// Tracks boolean flags for the page. Can be accessed concurrently with
    /// any latch.
    _Atomic uint8_t flags;
} header_t;

/// Mapping from a page id to a header pointer, stored in the hash map. The
/// page id is replicated to improve performance.
typedef struct {
    page_id_t page_id;
    header_t* header;
} page_t;

/// Linked list to track hash map overflows.
typedef struct bucket {
    page_t value;
    struct bucket* next;
} bucket_t;

/// Entry in the hash map directory. Provides two slots for immediate overflow
/// to improve performance.
typedef struct {
    /// Protects the hash map entry.
    latch_t latch;

    /// First slot for this hash map entry.
    page_t first;

    /// Second slot for this hash map entry.
    page_t second;

    /// Linked a list of further slots to track overflow.
    bucket_t* overflow;
} hash_entry_t;

/// Entry in the CLOCK ring. Mapping from a page id to a header pointer. Not
/// directly protected by any latch, but the header should only be modified
/// if the latch of the hash map entry for the page id was acquired.
typedef struct {
    _Atomic page_id_t page_id;
    header_t* header;
} ring_entry_t;

struct pager_t {
    /// Size of a single page in bytes.
    uint16_t page_size;

    /// The maximum number of pages tracked by the pager.
    uint32_t size;

    /// Mask used by the hash function.
    uint32_t mask;

    /// The number of pages currently tracked by the pager. Can be modified
    /// concurrently and might be an over approximation if the pager is
    /// accessed concurrently.
    _Atomic uint32_t page_count;

    /// Pointer into the ring, used for page eviction.
    _Atomic uint32_t evict_head;

    /// Pointer into the ring, used for page creation.
    _Atomic uint32_t create_head;

    hash_entry_t* directory;
    ring_entry_t* ring;
};

/// Converts a page header pointer to a pointer to the page data.
static unsigned char*
header_get_data(const header_t* page) {
    return (unsigned char*)(page + 1);
}

/// Converts a page data pointer to a pointer to the page header.
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

/// Simple hash function that maps the page id to hash map entries. (copied
/// from Stackoverflow but forgot the source)
static uint32_t
pager_hash(const pager_t* pager, const page_id_t id) {
    uint32_t hash = id;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash);

    return hash & pager->mask;
}

/// Retrieves the header for the corresponding page id from the entries.
/// Returns whether there exists a mapping for this page id in the entry.
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

/// Inserts a new mapping for this header into the entry. Might allocate new
/// buckets on overflow.
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

/// Removes a mapping from the entry. Does not deallocate any buckets and
/// leaves them to be reused on insertion.
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

/// Creates a new page. Might allocate a new page if no evicted page can be reused.
static result_t
pager_create(pager_t* pager, hash_entry_t* hash_entry, const page_id_t id, header_t** out) {
    assert_latch_write_access(hash_entry->latch);

    while (true) {
        const uint32_t index = atomic_fetch_add(&pager->create_head, 1) % pager->size;

        /// acquire the ring entry by storing the page, also ensures
        /// syncronisation since tha latch for the entry is alredy acquired
        ring_entry_t* ring_entry = &pager->ring[index];
        page_id_t expected = 0;
        if (!atomic_compare_exchange_weak(&ring_entry->page_id, &expected, id)) {
            continue;
        }

        if (ring_entry->header == nullptr) {
            ring_entry->header = malloc(sizeof(header_t) + pager->page_size);
        }
        if (ring_entry->header == nullptr) {
            atomic_store(&ring_entry->page_id, 0); // release ring entry on failure
            failure(ENOMEM, msg("failed to allocate memory for page"));
        }

        ring_entry->header->id = id;
        atomic_store(&ring_entry->header->flags, 0);
        latch_init(&ring_entry->header->latch);

        handle(pager_directory_insert(hash_entry, ring_entry->header)) {
            atomic_store(&ring_entry->page_id, 0); // release ring entry on failure
            forward();
        }

        *out = ring_entry->header;

        return SUCCESS;
    }
}

/// Finds the corresponding hash map entry for this ring entry. Returns true if
/// the ring entry stores a valid mapping (i.e. the page id is not zero). Since
/// the page id might be modified concurrently, a CAS loop is required.
static bool
pager_directory_find_entry(const pager_t* pager, ring_entry_t* ring_entry, hash_entry_t** out) {
    page_id_t page_id = atomic_load(&ring_entry->page_id);

    while (page_id != 0) {
        // acquire the latch of the corresponding hash map entry
        hash_entry_t* hash_entry = &pager->directory[pager_hash(pager, page_id)];
        latch_acquire_write(&hash_entry->latch);

        // check if the page id is still valid
        if (atomic_compare_exchange_strong(&ring_entry->page_id, &page_id, page_id)) {
            *out = hash_entry;
            return true;
        }

        // otherwise release the latch of the hash map entry and try again
        latch_release_write(&hash_entry->latch);
    }

    return false;
}

/// Evicts a page, might fail if there are no pages that can be evicted, i.e.
/// every page is currently fixed. Or if the thread is unlucky and loses a lot
/// of races with other threads trying to evict pages.
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

/// Looks up a page in the hash map entry and acquires the page latch if found.
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
