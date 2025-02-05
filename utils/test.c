#include <stdio.h>
#include "snow.h"

describe(error) {
    it("can create a test object") {
        asserteq(1, 1, "test");
    }
}

snow_main();
