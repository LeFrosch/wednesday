#pragma once

#include "utils/utils.h"

typedef struct {
    int handle;
    int parent;
} file_t;

result_t
create_file(const char* path, file_t* file);
