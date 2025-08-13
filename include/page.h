#pragma once

#include "error.h"

#include <stdlib.h>

typedef uint32_t page_id_t;
typedef unsigned char byte_t;

typedef struct {
    page_id_t id;
    byte_t* data;
} page_t;

result_t
page_init(page_t page, size_t size, uint8_t type);

result_t
page_insert(page_t page, uint16_t index, const byte_t* data, size_t size);

result_t
page_lookup(page_t page, uint16_t index, byte_t** out);
