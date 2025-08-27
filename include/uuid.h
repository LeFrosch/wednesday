#pragma once

#include "error.h"
#include "winter.h"

typedef unsigned char uuid_t[16];

result_t
uuid_v7_create(uuid_t out);

void
uuid_v7_package(uuid_t out, uint64_t timestamp, uint64_t rand_a, uint64_t rand_b);

TEST_ONLY void
uuid_v8_package(uuid_t out, uint32_t counter);

int
uuid_snprintf(char* str, size_t n, const uuid_t uuid);
