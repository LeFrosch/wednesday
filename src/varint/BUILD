load("//:defs.bzl", "we_library", "we_test")

we_library(
    name = "varint",
    srcs = ["varint.c"],
    hdrs = ["varint.h"],
    visibility = ["//src:__subpackages__"],
    deps = ["//src/utils"],
)

we_test(
    name = "test",
    srcs = ["varint_test.c"],
    deps = [
        ":varint",
        "//src/utils:test",
    ],
)
