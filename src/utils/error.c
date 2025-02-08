#include "error.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ERRORS 32
#define MAX_BUFFER_SIZE 8192

_Thread_local static char messages_buffer[MAX_BUFFER_SIZE];
_Thread_local static size_t messages_offset;
_Thread_local static size_t out_of_memory;

_Thread_local static const char* errors_messages[MAX_ERRORS];
_Thread_local static int32_t errors_codes[MAX_ERRORS];
_Thread_local static size_t errors_count;

__attribute__((__format__(__printf__, 4, 0))) static bool
try_vsprintf(char* buffer, const char* end, size_t* chars, const char* format, va_list args) {
    const size_t len = end - buffer;

    const int ret = vsnprintf(buffer, len, format, args);
    if (ret < 0) {
        return false; // encoding error
    }
    if ((size_t)ret >= len) {
        return false; // buffer overflow
    }

    *chars += ret + 1;

    return true;
}

__attribute__((__format__(__printf__, 4, 5))) static bool
try_sprintf(char* buffer, const char* end, size_t* chars, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const bool suc = try_vsprintf(buffer, end, chars, format, args);
    va_end(args);

    return suc;
}

void
errors_push_new(const char* file, const uint32_t line, const char* func, const int32_t code) {
    if (errors_count >= MAX_ERRORS || out_of_memory) {
        return;
    }

    const size_t index = errors_count++;
    errors_codes[index] = code;

    char error[128];
    const int ret = strerror_r(code, error, sizeof(error));
    if (ret) {
        strcpy(error, "Unknown error");
    }

    char* start = messages_buffer + messages_offset;
    const char* end = messages_buffer + MAX_BUFFER_SIZE;

    const bool suc = try_sprintf(start, end, &messages_offset, "%s(%s:%d) %s(%d)", func, file, line, error, code);
    if (suc) {
        errors_messages[index] = start;
    } else {
        out_of_memory = true;
    }
}

__attribute__((__format__(__printf__, 1, 2))) void
errors_append_message(const char* format, ...) {
    if (errors_count == 0 || out_of_memory) {
        return;
    }

    // overwrite the 0 byte again
    messages_offset -= 1;

    char* start = messages_buffer + messages_offset;
    const char* end = messages_buffer + MAX_BUFFER_SIZE;

    va_list args;
    va_start(args, format);
    const bool suc = try_vsprintf(start, end, &messages_offset, format, args);
    va_end(args);

    if (!suc) {
        out_of_memory = true;
    }
}

void
errors_print(void) {
    for (size_t i = 0; i < errors_count; i++) {
        printf("%s\n", errors_messages[i]);
    }

    if (out_of_memory) {
        printf("Out of memory, additional errors were discarded\n");
    }
}

void
errors_clear(void) {
    memset(messages_buffer, 0, sizeof(messages_buffer));
    messages_offset = 0;
    out_of_memory = false;

    memset(errors_messages, 0, sizeof(errors_messages));
    memset(errors_codes, 0, sizeof(errors_codes));
    errors_count = 0;
}

size_t
errors_get_count(void) {
    return errors_count;
}

const char*
errors_get_message(void) {
    if (errors_count == 0) {
        return NULL;
    }

    return errors_messages[0];
}

const char*
errors_get_trace(void) {
    if (errors_count == 0) {
        return NULL;
    }

    _Thread_local static char buffer[MAX_BUFFER_SIZE];

    char* head = buffer;
    for (size_t i = 0; i < errors_count; i++) {
        strcpy(head, errors_messages[i]);

        head += strlen(errors_messages[i]);
        *(head++) = '\n';
    }
    *(head - 1) = 0;

    return buffer;
}
