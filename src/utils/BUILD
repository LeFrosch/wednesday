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
    name = "math",
    hdrs = ["math.h"],
    deps = [":error"],
)

we_test(
    name = "math_test",
    srcs = ["math_test.c"],
    deps = [":math"],
)

we_library(
    name = "utils",
    hdrs = ["utils.h"],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":defer",
        ":error",
        ":math",
    ],
)

we_library(
    name = "test",
    testonly = 1,
    hdrs = [
        "test.h",
        "utils.h",
    ],
    visibility = ["//src:__subpackages__"],
    deps = [
        ":defer",
        ":error",
        ":math",
        "//third_party/snow:lib",
    ],
)
