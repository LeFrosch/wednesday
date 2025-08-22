#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "deffer.h"

typedef _Atomic int32_t latch_t;

void
latch_acquire_read(latch_t* latch);

void
latch_acquire_write(latch_t* latch);

void
latch_acquire(latch_t* latch, bool exclusive);

void
latch_release_read(latch_t* latch);

void
latch_release_write(latch_t* latch);

bool
latch_available(const latch_t* latch);

defer_impl(latch_release_read) {
    defer_guard();
    latch_release_read(defer_arg(latch_t));
}

defer_impl(latch_release_write) {
    defer_guard();
    latch_release_write(defer_arg(latch_t));
}

#define assert_latch_read_access(latch) assert(latch != 0)

#define assert_latch_write_access(latch) assert(latch < 0)
