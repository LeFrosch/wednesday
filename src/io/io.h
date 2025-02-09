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

result_t
file_write(const file_t* file, uint64_t offset, const char* data, size_t size);

result_t
file_read(const file_t* file, uint64_t offset, char* data, size_t size);

result_t
file_sync(const file_t* file);

enabel_defer(file_close, (file_t*), SUCCESS);
