#include <assert.h>
#include <stdatomic.h>

#include "latch.h"
#include "winter.h"

void
latch_init(latch_t* latch) {
    atomic_store_explicit(latch, 0, memory_order_seq_cst);
}

void
latch_acquire_read(latch_t* latch) {
    int32_t value = 0; // optimistic initialization, compare exchange will load anyway

    while (1) {
        if (value >= 0) {
            const bool success = atomic_compare_exchange_weak_explicit(
              latch, &value, value + 1, memory_order_acquire, memory_order_relaxed
            );

            if (success) {
                break;
            }
        } else {
            value = atomic_load_explicit(latch, memory_order_relaxed);
        }
    }
}

bool
latch_try_acquire_read(latch_t* latch) {
    int32_t value = 0; // optimistic initialization, compare exchange will load anyway

    while (1) {
        if (value < 0) {
            return false;
        }
        const bool success = atomic_compare_exchange_weak_explicit(
          latch, &value, value + 1, memory_order_acquire, memory_order_relaxed
        );

        if (success) {
            return true;
        }
    }
}

void
latch_acquire_write(latch_t* latch) {
    int32_t value = 0; // optimistic initialization, compare exchange will load anyway

    while (1) {
        if (value == 0) {
            const bool success = atomic_compare_exchange_weak_explicit(
              latch, &value, -1, memory_order_acquire, memory_order_relaxed
            );

            if (success) {
                break;
            }
        } else {
            value = atomic_load_explicit(latch, memory_order_relaxed);
        }
    }
}

bool
latch_try_acquire_write(latch_t* latch) {
    int32_t value = 0; // optimistic initialization, compare exchange will load anyway

    while (1) {
        if (value != 0) {
            return false;
        }
        const bool success = atomic_compare_exchange_weak_explicit(
          latch, &value, -1, memory_order_acquire, memory_order_relaxed
        );

        if (success) {
            return true;
        }
    }
}

void
latch_acquire(latch_t* latch, const bool exclusive) {
    if (exclusive) {
        latch_acquire_write(latch);
    } else {
        latch_acquire_read(latch);
    }
}

void
latch_release_read(latch_t* latch) {
    assert(atomic_load(latch) > 0);
    atomic_fetch_sub_explicit(latch, 1, memory_order_release);
}

void
latch_release_write(latch_t* latch) {
    assert(atomic_load(latch) == -1);
    atomic_store_explicit(latch, 0, memory_order_release);
}

bool
latch_available(const latch_t* latch) {
    return atomic_load_explicit(latch, memory_order_acquire) == 0;
}
