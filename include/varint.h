#pragma once

#include <stdint.h>

uint16_t
varint_put(unsigned char* dst, uint64_t value);

uint16_t
varint_put_len(uint64_t value);

uint16_t
varint_get(const unsigned char* src, uint64_t* out);

uint16_t
varint_get_len(const unsigned char* src);

int
varint_cmp(const unsigned char* a, const unsigned char* b);
