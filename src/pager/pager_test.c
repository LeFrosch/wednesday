#include "pager.h"
#include "snow.h"
#include "utils/test.h"

#define PAGE_SIZE 256
#define MAX_PAGES 20
#define EXTRA_SIZE 64

describe(pager) {
    before_each() {
        errors_clear();
    }

    it("can open and close pager") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES, EXTRA_SIZE));
        assert(pager != NULL);

        assertsuc(pager_close(&pager));
        assert(pager == NULL);
    }

    it("can get a new page") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES, EXTRA_SIZE));
        defer(pager_close, pager);

        unsigned char* page;
        assertsuc(pager_get(pager, 0, &page));
        assert(page != NULL);

        asserteq(pager_get_page_number(page), 0);
    }

    it("can get a cached page") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES, EXTRA_SIZE));
        defer(pager_close, pager);

        unsigned char* page_a;
        assertsuc(pager_get(pager, 0, &page_a));
        unsigned char* page_b;
        assertsuc(pager_get(pager, 0, &page_b));

        assert(page_a == page_b);
        asserteq(pager_get_page_number(page_a), 0);
        asserteq(pager_get_page_number(page_b), 0);
    }

    it("can get two new pages") {
        pager_t* pager;
        assertsuc(pager_open(&pager, PAGE_SIZE, MAX_PAGES, EXTRA_SIZE));
        defer(pager_close, pager);

        unsigned char* page_a;
        assertsuc(pager_get(pager, 0, &page_a));
        unsigned char* page_b;
        assertsuc(pager_get(pager, 1, &page_b));

        assert(page_a != page_b);
        asserteq(pager_get_page_number(page_a), 0);
        asserteq(pager_get_page_number(page_b), 1);
    }
}
