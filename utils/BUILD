load("//:defs.bzl", "we_library", "we_test")

we_library(
    name = "defer",
    hdrs = ["defer.h"],
)

we_library(
    name = "error",
    srcs = ["error.c"],
    hdrs = ["error.h"],
)

we_test(
    name = "error_test",
    srcs = ["error_test.c"],
    deps = [":error"],
)
