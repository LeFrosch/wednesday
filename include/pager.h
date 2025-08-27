#pragma once

#include "error.h"
#include "deffer.h"

#include <stdint.h>

/// The minimum alignment of a page guaranteed by the pager.
#define PAGE_ALIGNMENT 4

/// The unique id of a page. Zero is an invalid page ID.
typedef uint32_t page_id_t;

typedef struct {
    page_id_t id;
    unsigned char* data;
} page_t;

/// Thread safe page cache implementation. Uses a hash map for page lookups and
/// CLOCK ring with a retry bit for page eviction.
typedef struct pager_t pager_t;

/// Allocates and initialises a new pager. The total capacity of the pager is
/// about 70% of the hash map directory, and the directory size has to be a
/// power of 2.
result_t
pager_open(pager_t** out, uint16_t page_size, uint32_t directory_size);

/// Returns the size of a single page.
uint16_t
pager_get_page_size(const pager_t* pager);

/// Retrieves a page. With write lock if exclusive is set to true and with a
/// shared read lock if exclusive is set to false.
result_t
pager_fix(pager_t* pager, page_id_t id, bool exclusive, page_t* out);

/// Retrieves a new page with write lock.
result_t
pager_next(pager_t* pager, page_t* out);

/// Unfixes a page and releases the lock.
void
pager_unfix(page_t page);

defer_impl(pager_unfix) {
    defer_guard();
    pager_unfix(*defer_arg(page_t));
}

/// Closes the pager and frees all allocated pages. Should only be called if no
/// page is fixed any more. Otherwise, the behaviour is undefined.
result_t
pager_close(pager_t** out);
