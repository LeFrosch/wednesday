load("//:defs.bzl", "we_library", "we_test")

we_library(
    name = "btree",
    srcs = ["btree.c"],
    hdrs = ["btree.h"],
    deps = [
        "//src/pager",
        "//src/utils",
        "//src/uuid",
    ],
)

we_test(
    name = "test",
    srcs = ["btree_test.c"],
    deps = [
        ":btree",
        "//src/utils:test",
    ],
)
