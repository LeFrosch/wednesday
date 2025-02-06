load("//:defs.bzl", "we_library", "we_test")

we_library(
    name = "defer",
    hdrs = ["defer.h"],
    deps = [":error"],
)

we_test(
    name = "defer_test",
    srcs = ["defer_test.c"],
    deps = [":defer"],
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

we_library(
    name = "utils",
    hdrs = ["utils.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":defer",
        ":error",
    ],
)
