load("//:defs.bzl", "we_library", "we_test")

we_library(
    name = "uuid",
    srcs = ["uuid.c"],
    hdrs = ["uuid.h"],
    visibility = ["//src:__subpackages__"],
    deps = ["//src/utils"],
)

we_test(
    name = "test",
    srcs = ["uuid_test.c"],
    deps = [
        ":uuid",
        "//src/utils:test",
    ],
)
