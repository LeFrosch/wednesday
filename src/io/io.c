#include "io.h"
#include "utils/utils.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

result_t open_parent(const char* path, int* fd) {
    char* last = strrchr(path, '/');

    if (!last) {
        *fd = open(".", O_RDONLY);
    } else {
        char* directory = alloca(last - path + 1);
        strncpy(directory, path, last - path);

        *fd = open(path, O_RDONLY);
    }

    if (*fd == -1) {
        failure(errno, msg("could not open parent directory"), with(path, "%s"));
    }

    return SUCCESS;
}

result_t sync_parent(const char* path) {
    int fd;
    ensure(open_parent(path, &fd));
    defer(close, fd);

    if (!fsync(fd)) {
        failure(errno, msg("could not sync parent directory"), with(path, "%s"));
    }

    return SUCCESS;
}

result_t create_directories(const char* path) {
    const char* last = path;

    // if path starts from the root directory
    if (*last == '/') {
        last = strchr(last, '/');
    }

    while (last) {
        char* directory = alloca(last - path + 1);
        strncpy(directory, path, last - path);

        struct stat st;
        const int ret = stat(directory, &st);

        if (ret) {
            if (!S_ISDIR(st.st_mode)) {
                failure(errno, msg("expected a directory"), with(directory, "%s"));
            }
        } else {
            // the directory probably does not exist
            if (!mkdir(directory, S_IRWXU) && errno != EEXIST) {
                failure(errno, msg("cannot create directory"), with(directory, "%s"));
            }

            // ensure the new directory is persisted by syncing the parent
            ensure(sync_parent(directory));
        }

        last = strchr(last, '/');
    }

    return SUCCESS;
}

result_t create_file(const char* path, file_t* file) {
    ensure_no_errors();
    ensure(file);

    ensure(create_directories(path));

    file->handle = open(
        path,
        // open file for read and write, and close on fork
        O_CREAT | O_RDWR | O_CLOEXEC,
        // give the user read and write permission
        S_IRUSR | S_IWUSR
    );
    
    if (!file->handle) {
       failure(errno, msg("could not open file"), with(path, "%s"));
    }
    errdefer(close, file->handle);

    ensure(open_parent(path, &file->parent));
    errdefer(close, file->parent);

    if (fsync(file->parent)) {
        failure(errno, msg("could not sync parent directory"), with(path, "%s"));
    }

    return SUCCESS;
}
