#pragma once

#include "utils/utils.h"

typedef struct {
    int handle;
    int parent;
} file_t;

result_t
file_open(const char* path, file_t* file);

result_t
file_set_size(const file_t* file, uint64_t size);

result_t
file_close(const file_t* file);
