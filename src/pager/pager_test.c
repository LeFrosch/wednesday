#include "pager.h"
#include "snow.h"
#include "utils/test.h"

#define PAGE_SIZE 256
#define MAX_PAGES 10

describe(pager) {
    before_each() {
        errors_clear();
    }

    it("can open and close pager") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES));
        assert(pager != NULL);

        assertsuc(pager_close(pager));
    }

    it("can get a new page") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES));

        info_t* info;
        assertsuc(pager_get(pager, 0, &info));
        assert(info != NULL);

        asserteq(info->number, 0);

        assertsuc(pager_close(pager));
    }

    it("can get a cached page") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES));

        info_t* info_a;
        assertsuc(pager_get(pager, 0, &info_a));
        info_t* info_b;
        assertsuc(pager_get(pager, 0, &info_b));

        assert(info_a == info_b);

        assertsuc(pager_close(pager));
    }


    it("can get two new pages") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES));

        info_t* info_a;
        assertsuc(pager_get(pager, 0, &info_a));
        info_t* info_b;
        assertsuc(pager_get(pager, 1, &info_b));

        assert(info_a != info_b);

        assertsuc(pager_close(pager));
    }
}
