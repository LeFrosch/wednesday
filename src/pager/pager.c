#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "pager.h"

/**
 * Size of the hashtable to locate in memory pages by their page number. Should
 * be a multiple of 2 or the hash function breaks.
 */
#define PAGE_TABLE_SIZE 1024

/**
 * Hash a page number.
 */
#define page_table_hash(page_no) ((page_no) & (PAGE_TABLE_SIZE - 1))

// forward declaration because page_t is recursive
typedef struct page_t page_t;

/**
 * Stores all information tracked for one in memory page. Allways stored right
 * in front of the actual page data.
 */
struct page_t {
    info_t info;         /* the public information tracked for this page */
    page_t* list_next;   /* next page in the linked list */
    page_t* bucket_next; /* overflow chain for the hash map */
};

struct pager_t {
    uint32_t page_size;                  /* the size of one page */
    uint32_t max_pages;                  /* the maximum amount of pages in the cache */
    page_t* page_list;                   /* linked list of all pages */
    page_t* page_table[PAGE_TABLE_SIZE]; /* hash map from page number to pages */
};

result_t
pager_open(pager_t** out, const uint32_t page_size, const uint32_t max_pages) {
    ensure(out);
    ensure_no_errors();

    pager_t* pager = malloc(sizeof(pager_t));
    ensure(pager);
    memset(pager, 0, sizeof(pager_t)); // TODO: combin in one util method?

    pager->page_size = page_size;
    pager->max_pages = max_pages;

    *out = pager;
    return SUCCESS;
}

static page_t*
pager_lookup(const pager_t* pager, const uint32_t page_no) {
    page_t* page = pager->page_table[page_table_hash(page_no)];
    while (page && page->info.number != page_no) {
        page = page->list_next;
    }
    return page;
}

result_t
pager_get(pager_t* pager, const uint32_t page_no, info_t** out) {
    ensure(pager);
    ensure(out);
    ensure_no_errors();

    // try to look up the page, maybe it's already in the cache
    page_t* page = pager_lookup(pager, page_no);
    if (page) {
        *out = &page->info;
        return SUCCESS;
    }

    page = malloc(sizeof(page_t) + pager->page_size);
    ensure(page);

    // populate data fields
    page->info.number = page_no;
    page->info.data = (void*)&page[1];

    // insert the page into the linked list
    page->list_next = pager->page_list;
    pager->page_list = page;

    // insert the page into the hash table
    const size_t hash = page_table_hash(page_no);
    page->bucket_next = pager->page_table[hash];
    pager->page_table[hash] = page;

    *out = &page->info;
    return SUCCESS;
}

result_t
pager_close(pager_t* pager) {
    ensure(pager);
    ensure_no_errors();

    // free all pages
    page_t* page = pager->page_list;
    while (page) {
        page_t* next = page->list_next;
        free(page);
        page = next;
    }

    free(pager);

    return SUCCESS;
}
