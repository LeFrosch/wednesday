#include <string.h>
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
 * Stores information tracked for every in memory page. Allways stored right
 * in front of the actual page data.
 */
struct page_t {
    pager_t* pager;      /* the pager that owns this page */
    uint32_t number;     /* the page number to identify this page */
    void* extra_data;    /* extra data associated to page, used by the btree.c */
    page_t* list_next;   /* next page in the linked list */
    page_t* bucket_next; /* overflow chain for the hash map */
};

/**
 * Converts a page_t pointer to a pointer to the raw page data.
 */
#define page_to_data(ptr) ((unsigned char*)(page + 1))

/**
 * Converts a pointer to the raw page data to a page_t pointer.
 */
#define data_to_page(data) ((page_t*)(data) - 1)

struct pager_t {
    uint32_t page_size;                  /* the size of one page */
    uint32_t max_pages;                  /* the maximum amount of pages in the cache */
    uint32_t extra_size;                 /* the size of extra data associated with each page */
    uint32_t page_count;                 /* the number of pages in the entire database */
    page_t* page_list;                   /* linked list of all pages */
    page_t* page_table[PAGE_TABLE_SIZE]; /* hash map from page number to pages */
};

result_t
pager_open(pager_t** out, const uint32_t page_size, const uint32_t max_pages, const uint32_t extra_size) {
    ensure(out);
    ensure(max_pages > 10);
    ensure(page_size >= 256);
    ensure((page_size & (page_size - 1)) == 0);
    ensure_no_errors();

    pager_t* pager = malloc(sizeof(pager_t));
    ensure(pager);
    memset(pager, 0, sizeof(pager_t)); // TODO: combin in one util method?

    pager->page_size = page_size;
    pager->max_pages = max_pages;
    pager->extra_size = extra_size;
    pager->page_count = 0;

    *out = pager;
    return SUCCESS;
}

static page_t*
pager_lookup(const pager_t* pager, const uint32_t page_no) {
    page_t* page = pager->page_table[page_table_hash(page_no)];
    while (page && page->number != page_no) {
        page = page->list_next;
    }
    return page;
}

result_t
pager_get(pager_t* pager, const uint32_t page_no, unsigned char** out) {
    ensure(pager);
    ensure(out);
    ensure_no_errors();

    // try to look up the page, maybe it's already in the cache
    page_t* page = pager_lookup(pager, page_no);
    if (page) {
        *out = page_to_data(page);
        return SUCCESS;
    }

    page = malloc(sizeof(page_t) + pager->page_size);
    ensure(page);
    memset(page, 0, sizeof(page_t) + pager->page_size);
    errdefer(free, page);

    // increment the total page count
    pager->page_count++;

    // populate data fields
    page->number = page_no;
    page->pager = pager;

    // allocate the space for the extra data
    page->extra_data = malloc(pager->extra_size);
    ensure(page->extra_data);

    // insert the page into the linked list
    page->list_next = pager->page_list;
    pager->page_list = page;

    // insert the page into the hash table
    const size_t hash = page_table_hash(page_no);
    page->bucket_next = pager->page_table[hash];
    pager->page_table[hash] = page;

    *out = page_to_data(page);
    return SUCCESS;
}

pgno_t
pager_get_page_number(const unsigned char* page) {
    assert(page);
    return data_to_page(page)->number;
}

void*
pager_get_extra_data(const unsigned char* page) {
    assert(page);
    return data_to_page(page)->extra_data;
}

uint32_t
pager_get_page_size(pager_t* pager) {
    assert(pager);
    return pager->page_size;
}

uint32_t
pager_get_page_count(pager_t* pager) {
    assert(pager);
    return pager->page_count;
}

result_t
pager_close(pager_t** ptr) {
    ensure(ptr);
    ensure_no_errors();

    pager_t* pager = *ptr;
    ensure(pager);

    // free all pages
    page_t* page = pager->page_list;
    while (page) {
        page_t* next = page->list_next;
        free(page->extra_data);
        free(page);
        page = next;
    }

    free(pager);
    *ptr = NULL;

    return SUCCESS;
}
