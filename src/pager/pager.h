#pragma once

#include "utils/utils.h"

/**
 * Opaque pager type. The pager is definitely not thread safe.
 */
typedef struct pager_t pager_t;

/**
 * Public information tracked for every in memory pages.
 */
typedef struct {
    uint32_t number; /* the page number to identify this page */
    void* data;      /* pointer to the raw data of the page */
} info_t;

result_t
pager_open(pager_t** out, uint32_t page_size, uint32_t max_pages);

result_t
pager_get(pager_t* pager, uint32_t page_no, info_t** out);

result_t
pager_close(pager_t* pager);
