load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "defer",
    hdrs = ["defer.h"],
    copts = ["-std=c17", "-pedantic"]
)

cc_library(
    name = "error",
    srcs = ["error.c"],
    hdrs = ["error.h"],
    copts = ["-std=c17", "-pedantic"]
)

cc_test(
    name = "defer_test",
    size = "small",
    srcs = ["test.c"],
    copts = ["-std=gnu17", "-pedantic", "-Wno-strict-prototypes"],
    deps = [
        ":error",
        "@snow//:snow",
    ],
)
