#pragma once

#include <stdlib.h>
#include <stdint.h>

size_t
varint_get(const unsigned char* buf, uint64_t* out);

size_t
varint_put(unsigned char* buf, uint64_t val);

size_t
varint_len(uint64_t val);
