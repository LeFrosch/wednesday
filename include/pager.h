#pragma once

#include <stdint.h>

/// The unique id of a page. Zero is an invalid page ID.
typedef uint32_t page_id_t;

/// Thread safe page cache implementation. Uses a hash map for page lookups and
/// CLOCK ring with a retry bit for page eviction.
typedef struct pager_t pager_t;

/// Initialises a new pager. The total capacity of the pager is about 70% of
/// the hash map directory, and the directory size has to be a power of 2.
result_t
pager_open(pager_t* pager, uint16_t page_size, uint32_t directory_size);

/// Retrieves a new page. With write lock if exclusive is set to true and with
/// a shared read lock if exclusive is set to false.
result_t
pager_fix(pager_t* pager, page_id_t id, bool exclusive, unsigned char** out);

/// Unfixes a page and releases the lock.
void
pager_unfix(const unsigned char* data);

/// Closes the pager and frees all allocated pages. Should only be called if no
/// page is fixed any more. Otherwise, the behaviour is undefined.
result_t
pager_close(pager_t* pager);
