#include "uuid.h"

#include <stdlib.h>
#include <sys/time.h>

result_t
uuid_v7_create(uuid_t out) {
    ensure(out != nullptr);

    struct timeval now;
    if (gettimeofday(&now, NULL) != 0) {
        failure(errno, msg("cannot get time of day"));
    }

    const int64_t milliseconds = now.tv_sec * 1000L + now.tv_usec / 1000L;
    const int64_t rand_a = random();
    const int64_t rand_b = random() << 32 | random();

    uuid_v7_package(out, (uint64_t)milliseconds, (uint64_t)rand_a, (uint64_t)rand_b);

    return SUCCESS;
}

static inline void
intcpy(unsigned char* dst, const uint64_t src, const size_t n) {
    for (size_t i = 0; i < n; i++) {
        dst[i] = (unsigned char)(src >> (8 * (n - i - 1)));
    }
}

void
uuid_v7_package(uuid_t out, const uint64_t timestamp, const uint64_t rand_a, const uint64_t rand_b) {
    intcpy(out + 0, timestamp, 6);
    intcpy(out + 6, rand_a, 2);
    intcpy(out + 8, rand_b, 8);

    // set version to 7
    out[6] = (out[6] & 0x0f) | (7 << 4);

    // set variant to 2
    out[8] = (out[8] & 0x3f) | (2 << 6);
}

TEST_ONLY void
uuid_v8_package(uuid_t out, const uint32_t counter) {
    memcpy(out, &counter, 4);
    intcpy(out + 4, 0, 4);
    intcpy(out + 8, -1ULL, 8);

    // set version to 8
    out[6] = (out[6] & 0x0f) | (8 << 4);

    // set variant to 0
    out[8] = (out[8] & 0x3f) | (0 << 6);
}

int
uuid_snprintf(char* str, const size_t n, const uuid_t uuid) {
    return snprintf(
      str,
      n,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid[0],
      uuid[1],
      uuid[2],
      uuid[3],
      uuid[4],
      uuid[5],
      uuid[6],
      uuid[7],
      uuid[8],
      uuid[9],
      uuid[10],
      uuid[11],
      uuid[12],
      uuid[13],
      uuid[14],
      uuid[15]
    );
}
