#include "math.h"
#include "snow.h"

#include "utils/error.h"

#define test_sub(a, b, res_type, result)                                                                               \
    it("ckd_sub(" #a ", " #b ", (" #res_type ")) == " #result) {                                                       \
        res_type res;                                                                                                  \
        asserteq(ckd_sub(a, b, &res), result);                                                                         \
    }

describe(math) {
    test_sub((uint8_t)0, (uint8_t)0, uint8_t, SUCCESS)
    test_sub((uint8_t)0, (uint8_t)1, uint8_t, FAILURE)
    test_sub((uint16_t)300, (uint16_t)1, uint8_t, FAILURE)
}
