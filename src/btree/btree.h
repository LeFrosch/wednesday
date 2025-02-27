#pragma once

#include "pager/pager.h"
#include "utils/utils.h"
#include "uuid/uuid.h"

result_t
btree_open(pager_t** out, uint32_t page_cache_size);

result_t
btree_store_create(pager_t* pager, pgno_t* out);
