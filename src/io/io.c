#include "io.h"
#include "utils/utils.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

static char*
strncpy0(char* dst, const char* src, size_t n) {
    strncpy(dst, src, n);
    dst[n] = 0;
    return dst;
}

static result_t
open_parent(const char* path, int* fd) {
    char* last = strrchr(path, '/');

    if (!last) {
        *fd = open(".", O_RDONLY);
    } else {
        char* directory = alloca(last - path + 1);
        strncpy0(directory, path, last - path);

        *fd = open(directory, O_RDONLY);
    }

    if (*fd == -1) {
        failure(errno, msg("could not open parent directory"), with_str(path));
    }

    return SUCCESS;
}

static result_t
sync_parent(const char* path) {
    int fd;
    ensure(open_parent(path, &fd));
    defer(close, fd);

    if (fsync(fd) != 0) {
        failure(errno, msg("could not sync parent directory"), with_str(path));
    }

    return SUCCESS;
}

static result_t
create_directories(const char* path) {
    const char* last = strchr(path, '/');

    while (last) {
        char* directory = alloca(last - path + 1);
        strncpy0(directory, path, last - path);

        struct stat st;
        const int ret = stat(directory, &st);

        if (ret == 0) {
            if (!S_ISDIR(st.st_mode)) {
                failure(errno, msg("expected a directory"), with_str(directory));
            }
        } else {
            // the directory probably does not exist
            if (mkdir(directory, S_IRWXU) != 0 && errno != EEXIST) {
                failure(errno, msg("cannot create directory"), with_str(directory));
            }

            // ensure the new directory is persisted by syncing the parent
            ensure(sync_parent(directory));
        }

        last = strchr(last + 1, '/');
    }

    return SUCCESS;
}

result_t
file_open(file_t* file, const char* path) {
    ensure(path);
    ensure(file);
    ensure_no_errors();

    ensure(create_directories(path));

    // flags: O_DIRECT | O_DSYNC would allow for durable writes, no need for fsync

    file->handle = open(
      path,
      // open file for read and write, and close on fork
      O_CREAT | O_RDWR | O_CLOEXEC,
      // give the user read and write permission
      S_IRUSR | S_IWUSR);

    if (file->handle == -1) {
        failure(errno, msg("could not open file"), with_str(path));
    }
    errdefer(close, file->handle);

    ensure(open_parent(path, &file->parent));
    errdefer(close, file->parent);

    // sync the parent directory to ensure the created file is persisted
    if (fsync(file->parent) != 0) {
        failure(errno, msg("could not sync parent directory"), with_str(path));
    }

    return SUCCESS;
}

result_t
file_set_size(const file_t* file, const uint64_t size) {
    ensure(file);
    ensure_no_errors();

    if (ftruncate(file->handle, (off_t)size) != 0) {
        failure(errno, msg("could not truncate file"), with_size(size));
    }

    if (fsync(file->parent) != 0) {
        failure(errno, msg("could not sync parent directory"));
    }

    return SUCCESS;
}

result_t
file_close(const file_t* file) {
    ensure(file);
    ensure_no_errors();

    if (close(file->handle) != 0) {
        failure(errno, msg("could not close file"));
    }
    if (close(file->parent) != 0) {
        failure(errno, msg("could not close parent of file"));
    }

    return SUCCESS;
}

result_t
file_write(const file_t* file, uint64_t offset, const char* data, size_t size) {
    ensure(file);
    ensure(data);
    ensure_no_errors();

    while (size > 0) {
        const ssize_t ret = pwrite(file->handle, data, size, (off_t)offset);
        if (ret == -1) {
            failure(errno, msg("could not write to file"), with_size(size), with_size(offset));
        }

        size -= (size_t)ret;
        data += ret;
        offset += (uint64_t)ret;
    }

    return SUCCESS;
}

result_t
file_read(const file_t* file, uint64_t offset, char* data, size_t size) {
    ensure(file);
    ensure(data);
    ensure_no_errors();

    while (size > 0) {
        const ssize_t ret = pread(file->handle, data, size, (off_t)offset);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }

            failure(errno, msg("could not read from file"), with_size(size), with_size(offset));
        }
        if (ret == 0) {
            failure(EINVAL, msg("unexpected end of file"), with_size(size), with_size(offset));
        }

        size -= (size_t)ret;
        data += ret;
        offset += (uint64_t)ret;
    }

    return SUCCESS;
}

result_t
file_sync(const file_t* file) {
    ensure(file);
    ensure_no_errors();

    // TODO: use fdatasync on linux
    // TODO: use fcntl(..., F_FULLFSYNC) on mac
    if (fsync(file->handle) != 0) {
        failure(errno, msg("could not sync file"));
    }

    return SUCCESS;
}
