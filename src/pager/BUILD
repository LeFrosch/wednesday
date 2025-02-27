load("//:defs.bzl", "we_library", "we_test")

we_library(
    name = "pager",
    srcs = ["pager.c"],
    hdrs = ["pager.h"],
    visibility = ["//src:__subpackages__"],
    deps = ["//src/utils"],
)

we_test(
    name = "test",
    srcs = ["pager_test.c"],
    deps = [
        ":pager",
        "//src/utils:test",
    ],
)
