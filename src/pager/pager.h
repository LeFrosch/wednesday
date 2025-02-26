#pragma once

#include "utils/utils.h"

/**
 * Opaque pager type. The pager is definitely not thread safe.
 */
typedef struct pager_t pager_t;

result_t
pager_open(pager_t** out, uint32_t page_size, uint32_t max_pages, uint32_t extra_size);

result_t
pager_get(pager_t* pager, uint32_t page_no, unsigned char** out);

uint32_t
pager_get_page_number(const unsigned char* page);

void*
pager_get_extra_data(const unsigned char* page);

result_t
pager_close(pager_t** ptr);

enabel_defer(pager_close, (pager_t**), SUCCESS);
