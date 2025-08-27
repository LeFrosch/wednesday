#pragma once

#include <stdint.h>

#include "winter.h"

typedef struct {
    uint64_t size;
    unsigned char* data;
} blob_t;

int
blob_cmp(blob_t a, blob_t b);

uint64_t
blob_put(unsigned char* dst, blob_t blob);

uint64_t
blob_put_len(blob_t blob);

uint64_t
blob_get(unsigned char* src, blob_t* out);

uint64_t
blob_get_len(const unsigned char* src);

TEST_ONLY blob_t
blob_from_string(const char* value);
