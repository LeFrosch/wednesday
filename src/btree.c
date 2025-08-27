#include "btree.h"

#include "blob.h"
#include "deffer.h"
#include "error.h"
#include "pager.h"
#include "uuid.h"
#include "varint.h"
#include "winter.h"

#include <assert.h>

static int
key_compare(const unsigned char* a, const unsigned char* b) {
    uint64_t a_value, b_value;
    varint_get(a, &a_value);
    varint_get(b, &b_value);

    return (int)(a_value - b_value);
}

enum {
    PAGE_FLAG_LEAF = (1u << 0),
    PAGE_FLAG_TABLE = (1u << 1),
    PAGE_FLAG_INDEX_UUID = (1u << 2),
};

#define page_is_leaf(flags) (((flags) & PAGE_FLAG_LEAF) != 0)
#define page_is_inner(flags) (((flags) & PAGE_FLAG_LEAF) == 0)
#define page_is_table(flags) (((flags) & PAGE_FLAG_TABLE) != 0)
#define page_is_index_uuid(flags) (((flags) & PAGE_FLAG_INDEX_UUID) != 0)

static uint16_t
page_flags_package(const bool leaf, const uint16_t type) {
    return (leaf ? PAGE_FLAG_LEAF : 0) | type;
}

typedef struct {
    uint16_t cell_count;
    uint16_t data_start;
    uint16_t free_space;
    uint16_t flags;
    page_id_t right;
} header_t;

struct btree_t {
    pager_t* pager;
    page_id_t root;

    uint16_t page_size;
};

static page_id_t
payload_get_page_id(const unsigned char* payload) {
    page_id_t id;
    memcpy(&id, payload, sizeof(page_id_t));
    return id;
}

static uint16_t
payload_get_key_len(const uint16_t flags, const unsigned char* key) {
    if (page_is_index_uuid(flags)) {
        return sizeof(uuid_t);
    } else /* if(page_is_table(flags)) */ {
        return varint_get_len(key);
    }
}

static unsigned char*
payload_get_key_ptr(const uint16_t flags, unsigned char* payload) {
    if (page_is_inner(flags)) {
        return payload + sizeof(page_id_t);
    } else /* if(page_is_leaf(flags)) */ {
        return payload;
    }
}

static blob_t
payload_get_key(const uint16_t flags, unsigned char* payload) {
    unsigned char* data = payload_get_key_ptr(flags, payload);
    return (blob_t){ payload_get_key_len(flags, data), data };
}

static unsigned char*
payload_get_value_ptr(const uint16_t flags, unsigned char* payload) {
    assert(page_is_leaf(flags));

    if (page_is_index_uuid(flags)) {
        return payload + sizeof(uuid_t);
    } else /* if(page_is_table(flags)) */ {
        return payload + varint_get_len(payload);
    }
}

static uint16_t
payload_get_value_len(const uint16_t flags, const unsigned char* value) {
    assert(page_is_leaf(flags));

    if (page_is_index_uuid(flags)) {
        return sizeof(page_id_t);
    } else /* if(page_is_table(flags)) */ {
        return (uint16_t) blob_get_len(value);
    }
}

static uint16_t
payload_get_len(const uint16_t flags, const unsigned char* payload) {
    if (page_is_inner(flags)) {
        return payload_get_key_len(flags, payload + sizeof(page_id_t)) + sizeof(page_id_t);
    } else /* if(page_is_leaf(flags)) */ {
        const uint16_t key_len = payload_get_key_len(flags, payload);
        return key_len + payload_get_value_len(flags, payload + key_len);
    }
}

static uint16_t
payload_put_len(const uint16_t flags, const blob_t key, const blob_t value) {
    if (page_is_inner(flags)) {
        return (uint16_t) key.size + sizeof(page_id_t);
    } else /* if(page_is_leaf(flags)) */ {
        return (uint16_t) key.size  + (uint16_t)value.size;
    }
}

static header_t*
page_get_header(unsigned char* page) {
    return (header_t*)page;
}

static uint16_t*
page_get_cells(unsigned char* page) {
    return (uint16_t*)(page + sizeof(header_t));
}

static uint16_t
page_get_space(unsigned char* page) {
    const header_t* header = page_get_header(page);
    return header->data_start - sizeof(header_t) - sizeof(uint16_t) * header->cell_count;
}

static unsigned char*
page_get_payload(unsigned char* page, const uint16_t index) {
    return page + page_get_cells(page)[index];
}

result_t
btree_open(btree_t** out, pager_t* pager, const page_id_t root) {
    ensure(out != nullptr);
    ensure(pager != nullptr);

    btree_t* btree;
    try_alloc(btree, sizeof(btree_t));
    errdefer(free, btree);

    btree->pager = pager;
    btree->page_size = pager_get_page_size(pager);
    btree->root = root;

    *out = btree;

    return SUCCESS;
}

result_t
btree_create(btree_t** out, pager_t* pager, uint16_t type) {
    ensure(out != nullptr);
    ensure(pager != nullptr);

    page_t root;
    try(pager_next(pager, &root));
    defer(pager_unfix, root);

    header_t* header = page_get_header(root.data);
    header->cell_count = 0;
    header->data_start = pager_get_page_size(pager);
    header->free_space = pager_get_page_size(pager) - sizeof(header_t);
    header->flags = page_flags_package(true, type);
    header->right = 0;

    return btree_open(out, pager, root.id);
}

bool
page_find_pointer(unsigned char* page, const blob_t key, uint16_t* out) {
    const header_t* header = page_get_header(page);

    for (uint16_t i = 0; i < header->cell_count; ++i) {
        unsigned char* payload = page_get_payload(page, i);
        const int ret = key_compare(payload_get_key_ptr(header->flags, payload), key.data);

        if (ret == 0) {
            *out = i;
            return true;
        }
        if (ret > 0) {
            *out = i;
            return false;
        }
    }

    *out = header->cell_count;
    return false;
}

static void
page_compact(unsigned char* page, const uint16_t page_size) {
    header_t* header = page_get_header(page);
    uint16_t* pointers = page_get_cells(page);

    unsigned char buffer[page_size];
    uint16_t data_start = page_size;

    for (uint16_t i = 0; i < header->cell_count; ++i) {
        const unsigned char* payload = page_get_payload(page, i);
        const uint16_t size = payload_get_len(header->flags, payload);

        data_start -= size;
        memcpy(buffer + data_start, payload, size);

        pointers[i] = data_start;
    }

    memcpy(page + data_start, buffer + data_start, page_size - data_start);

    header->data_start = data_start;
    header->free_space = page_size - data_start - sizeof(header_t) - sizeof(uint16_t) * header->cell_count;
}

static void
page_insert_payload(
  unsigned char* page,
  const uint16_t page_size,
  const uint16_t index,
  const uint64_t payload_size,
  unsigned char** out
) {
    header_t* header = page_get_header(page);

    const uint64_t total_size = payload_size + sizeof(uint16_t);
    assert(header->free_space >= total_size);

    if (page_get_space(page) < total_size) {
        page_compact(page, page_size);
    }

    uint16_t* pointers = page_get_cells(page);
    if (index != header->cell_count) {
        memmove(pointers + index + 1, pointers + index, sizeof(uint16_t) * (header->cell_count - index));
    }

    const uint16_t payload_start = (uint16_t)(header->data_start - payload_size);
    pointers[index] = payload_start;

    header->data_start = payload_start;
    header->free_space -= total_size;
    header->cell_count += 1;

    *out = page + payload_start;
}

static result_t
page_insert_leaf(unsigned char* page, const uint16_t page_size, const blob_t key, const blob_t value) {
    uint16_t index;
    if (page_find_pointer(page, key, &index)) {
        failure(EEXIST, msg("key already exists on leaf page"));
    }

    unsigned char* ptr;
    page_insert_payload(page, page_size, index, key.size + value.size, &ptr);

    memcpy(ptr, key.data, key.size);
    // payload_put_value(page_get_header(page)->flags, ptr + key.size, value);
    memcpy(ptr + key.size, value.data, value.size);

    return SUCCESS;
}

static void
page_insert_inner(
  unsigned char* page,
  const uint16_t page_size,
  const uint16_t index,
  const blob_t key,
  const page_id_t page_id
) {
    header_t* header = page_get_header(page);

    unsigned char* ptr;
    if (index == header->cell_count) {
        page_insert_payload(page, page_size, index, key.size + sizeof(page_id_t), &ptr);
        memcpy(ptr, &header->right, sizeof(page_id_t));
        memcpy(ptr + sizeof(page_id_t), key.data, key.size);
        header->right = page_id;
    } else {
        unsigned char* next = page_get_payload(page, index);
        page_insert_payload(page, page_size, index, key.size + sizeof(page_id_t), &ptr);
        memcpy(ptr, next, sizeof(page_id_t));
        memcpy(ptr + sizeof(page_id_t), key.data, key.size);
        memcpy(next, &page_id, sizeof(page_id_t));
    }
}

static uint16_t
page_split(const page_t page, const page_t next, const uint16_t page_size) {
    header_t* page_header = page_get_header(page.data);
    header_t* next_header = page_get_header(next.data);
    uint16_t* next_pointers = page_get_cells(next.data);

    uint16_t next_data_start = page_size;

    const uint16_t split = page_header->cell_count / 2;
    for (uint16_t i = split; i < page_header->cell_count; ++i) {
        const unsigned char* payload = page_get_payload(page.data, i);
        const uint16_t size = payload_get_len(page_header->flags, payload);

        next_data_start -= size;
        memcpy(next.data + next_data_start, payload, size);

        next_pointers[i - split] = next_data_start;
    }

    next_header->flags = page_header->flags;
    next_header->cell_count = page_header->cell_count - split;
    next_header->data_start = next_data_start;
    next_header->free_space = next_data_start - sizeof(header_t) - sizeof(uint16_t) * next_header->cell_count;
    next_header->right = page_header->right;

    page_header->cell_count = split;
    page_header->right = next.id;
    page_compact(page.data, page_size);

    return split;
}

result_t
btree_insert_walk(const btree_t* btree, page_t parent, const blob_t key, const blob_t value) {
    const header_t* parent_header = page_get_header(parent.data);
    assert(page_is_inner(parent_header->flags));
    assert(parent_header->free_space >= payload_put_len(parent_header->flags, key, value));

    page_t page;
    { // parent lock scope
        defer(pager_unfix, parent);

        uint16_t index;
        if (page_find_pointer(parent.data, key, &index)) {
            failure(EEXIST, msg("key already exists on page"));
        }

        page_id_t next;
        if (index == parent_header->cell_count) {
            next = parent_header->right;
        } else {
            next = payload_get_page_id(page_get_payload(parent.data, index));
        }
        assert(next != 0);

        try(pager_fix(btree->pager, next, true, &page));

        // split the page if there might be not enough space
        const header_t* header = page_get_header(page.data);
        if (header->free_space < payload_put_len(header->flags, key, value)) {
            page_t split;
            try(pager_next(btree->pager, &split));

            const uint16_t split_index = page_split(page, split, btree->page_size);
            const blob_t split_key = payload_get_key(header->flags, page_get_payload(page.data, split_index - 1));

            page_insert_inner(parent.data, btree->page_size, index, split_key, split.id);

            if (key_compare(key.data, split_key.data) < 0) {
                pager_unfix(split);
            } else {
                pager_unfix(page);
                page = split;
            }
        }
    }

    const header_t* header = page_get_header(page.data);
    if (page_is_leaf(header->flags)) {
        defer(pager_unfix, page);
        try(page_insert_leaf(page.data, btree->page_size, key, value));
    } else {
        try(btree_insert_walk(btree, page, key, value));
    }

    return SUCCESS;
}

result_t
btree_insert(btree_t* btree, const blob_t key, const blob_t value) {
    page_t root;
    try(pager_fix(btree->pager, btree->root, true, &root));
    errdefer(pager_unfix, root);

    const header_t* header = page_get_header(root.data);
    if (header->free_space < payload_put_len(header->flags, key, value)) {
        page_t split;
        try(pager_next(btree->pager, &split));
        // TODO: this page will be unreachable on error

        const uint16_t split_index = page_split(root, split, btree->page_size);
        const blob_t split_key = payload_get_key(header->flags, page_get_payload(root.data, split_index - 1));

        page_t new;
        try(pager_next(btree->pager, &new));
        defer(pager_unfix, new);

        header_t* new_header = page_get_header(new.data);
        new_header->flags = header->flags & ~PAGE_FLAG_LEAF;
        new_header->cell_count = 1;
        new_header->data_start = btree->page_size - (sizeof(page_id_t) + (uint16_t) split_key.size);
        new_header->free_space = new_header->data_start - sizeof(header_t) - sizeof(uint16_t);
        new_header->right = split.id;

        uint16_t* new_pointers = page_get_cells(new.data);
        new_pointers[0] = new_header->data_start;

        unsigned char* new_cell = page_get_payload(new.data, 0);
        memcpy(new_cell, &root.id, sizeof(page_id_t));
        memcpy(new_cell + sizeof(page_id_t), split_key.data, split_key.size);

        // TODO: this needs to be synchronized
        btree->root = new.id;

        if (key_compare(key.data, split_key.data) < 0) {
            pager_unfix(split);
        } else {
            pager_unfix(root);
            root = split;
        }
    }

    if (page_is_leaf(page_get_header(root.data)->flags)) {
        defer(pager_unfix, root);
        try(page_insert_leaf(root.data, btree->page_size, key, value));
    } else {
        try(btree_insert_walk(btree, root, key, value));
    }

    return SUCCESS;
}

result_t
btree_lookup(const btree_t* btree, const blob_t key, unsigned char** out) {
    page_t page;
    try(pager_fix(btree->pager, btree->root, true, &page));
    defer(pager_unfix, page);

    while (page_is_inner(page_get_header(page.data)->flags)) {
        uint16_t index;
        page_find_pointer(page.data, key, &index);

        const header_t* header = page_get_header(page.data);

        page_id_t next;
        if (index == header->cell_count) {
            next = header->right;
        } else {
            next = payload_get_page_id(page_get_payload(page.data, index));
        }
        assert(next != 0);

        page_t child;
        try(pager_fix(btree->pager, next, true, &child));
        pager_unfix(page);

        page = child;
    }

    uint16_t index = 0;
    if (!page_find_pointer(page.data, key, &index)) {
        failure(ENOENT, msg("key not found on leaf page"));
    }

    const uint16_t flags = page_get_header(page.data)->flags;
    *out = payload_get_value_ptr(flags, page_get_payload(page.data, index));

    return SUCCESS;
}

result_t
btree_table_insert(btree_t* btree, const uint64_t id, const blob_t value) {
    unsigned char key_buf[9];
    const uint64_t key_len = varint_put(key_buf, id);

    // TODO: this is unsave :c
    unsigned char value_buf[blob_put_len(value)];
    const uint64_t value_len = blob_put(value_buf, value);

    return btree_insert(btree, (blob_t){ key_len, key_buf }, (blob_t){ value_len, value_buf });
}

result_t
btree_table_lookup(const btree_t* btree, const uint64_t id, blob_t* out) {
    unsigned char key[9];
    const uint16_t key_len = varint_put(key, id);

    unsigned char* value;
    try(btree_lookup(btree, (blob_t){ key_len, key }, &value));

    blob_get(value, out);

    return SUCCESS;
}

result_t
btree_close(btree_t** out) {
    ensure(out != nullptr);

    btree_t* btree = *out;

    free(btree);
    *out = nullptr;

    return SUCCESS;
}

static header_t
test_get_root_header(const btree_t* btree) {
    page_t root;
    assert_success(pager_fix(btree->pager, btree->root, false, &root));
    defer(pager_unfix, root);

    return *page_get_header(root.data);
}

describe(btree_table) {

    static uint16_t page_size = 1024;

    static pager_t* pager;
    static btree_t* btree;

    static blob_t value;
    static uint16_t inner_cell_count;
    static uint16_t leaf_cell_count;

    before_each() {
        value = blob_from_string("hello world");

        inner_cell_count = (page_size - sizeof(header_t)) / (sizeof(uint16_t) + sizeof(page_id_t) + 1);
        leaf_cell_count = (page_size - sizeof(header_t)) / (sizeof(uint16_t) + 1 + blob_put_len(value));

        assert_success(pager_open(&pager, page_size, 512));
        assert_success(btree_create(&btree, pager, PAGE_FLAG_TABLE));
    }

    after_each() {
        assert_success(btree_close(&btree));
        assert_success(pager_close(&pager));
        error_clear();
    }

    it("alignment") {
        // make sure the alignment of the header is compatible with the
        // guaranteed alignment of the page
        assertis(PAGE_ALIGNMENT % alignof(header_t) == 0);
    }

    it("insert into root leaf") {
        assert_success(btree_table_insert(btree, 7, value));

        blob_t result;
        assert_success(btree_table_lookup(btree, 7, &result));
        asserteq_int(blob_cmp(result, value), 0);
    }

    it("fill root leaf") {
        for (uint16_t i = 0; i < leaf_cell_count; ++i) {
            assert_success(btree_table_insert(btree, i, value));
        }

        const header_t header = test_get_root_header(btree);
        assertis(page_is_leaf(header.flags));
        asserteq_uint(header.cell_count, leaf_cell_count);
        asserteq_uint(header.right, 0);

        for (uint16_t i = 0; i < leaf_cell_count; ++i) {
            blob_t result;
            assert_success(btree_table_lookup(btree, i, &result));
            asserteq_int(blob_cmp(result, value), 0);
        }
    }

    it("split root leaf") {
        for (uint16_t i = 0; i < leaf_cell_count; ++i) {
            assert_success(btree_table_insert(btree, i, value));
        }

        {
            assert_success(btree_table_insert(btree, leaf_cell_count, value));

            const header_t header = test_get_root_header(btree);
            assertis(page_is_inner(header.flags));
            asserteq_uint(header.cell_count, 1);
            assertneq_uint(header.right, 0);
        }

        for (uint16_t i = 0; i <= leaf_cell_count; ++i) {
            blob_t result;
            assert_success(btree_table_lookup(btree, i, &result));
            asserteq_int(blob_cmp(result, value), 0);
        }
    }

    it("split inner node") {
        for (uint16_t i = 0; i < leaf_cell_count * inner_cell_count; ++i) {
            assert_success(btree_table_insert(btree, i, value));
        }

        for (uint16_t i = 0; i < leaf_cell_count * inner_cell_count; ++i) {
            blob_t result;
            assert_success(btree_table_lookup(btree, i, &result));
            asserteq_int(blob_cmp(result, value), 0);
        }
    }

    // parallel("split inner node", 8) {
    //     const uint16_t per_thread = (leaf_cell_count * inner_cell_count) / 8;

    //     for (uint16_t i = 0; i < per_thread; ++i) {
    //         assert_success(btree_table_insert(btree, i + per_thread * thread_index(), value));
    //     }

    //     for (uint16_t i = 0; i < per_thread; ++i) {
    //         blob_t result;
    //         assert_success(btree_table_lookup(btree, i + per_thread * thread_index(), &result));
    //         asserteq_int(blob_cmp(result, value), 0);
    //     }
    // }
}
