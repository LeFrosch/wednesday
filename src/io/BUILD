load("//:defs.bzl", "we_library", "we_test")

we_library(
    name = "io",
    srcs = ["io.c"],
    hdrs = ["io.h"],
    deps = ["//src/utils"],
)

we_test(
    name = "io_test",
    srcs = ["io_test.c"],
    deps = [
        ":io",
        "//src/utils:test",
    ],
)
