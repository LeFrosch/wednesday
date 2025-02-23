#include "snow.h"
#include "utils/test.h"
#include "uuid.h"

describe(uuid) {
    before_each() {
        errors_clear();
    }

    it("can print 0 uuid") {
        uuid_t uuid;
        memset(uuid, 0, sizeof(uuid_t));

        char buf[40];
        uuid_v7_snprintf(buf, 40, uuid);
        asserteq_str(buf, "00000000-0000-0000-0000-000000000000");
    }

    it("can package a uuid") {
        uuid_t uuid;
        uuid_v7_package(uuid, 1740342088566, 0xffff, 0xabcdabcdabcdabcd);

        char buf[40];
        uuid_v7_snprintf(buf, 40, uuid);
        asserteq_str(buf, "01953478-d376-7fff-abcd-abcdabcdabcd");
    }

    it("can create a uuid") {
        uuid_t uuid;
        assertsuc(uuid_v7_create(uuid));
    }
}
