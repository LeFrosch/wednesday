#include "btree.h"
#include "snow.h"
#include "utils/test.h"

#define CACHE_SIZE 20

describe(btree) {
    before_each() {
        errors_clear();
    }

    it("can open and close a pager") {
        pager_t* pager;
        assertsuc(btree_open(&pager, CACHE_SIZE));
        assert(pager != NULL);

        assertsuc(pager_close(&pager));
        assert(pager == NULL);
    }

    it("can create a store") {
        pager_t* pager;
        assertsuc(btree_open(&pager, CACHE_SIZE));
        defer(pager_close, pager);

        pgno_t index;
        assertsuc(btree_store_create(pager, &index));

        assert(index == 0);
    }
}
