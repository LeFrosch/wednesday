load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

COPTS = [
    "-std=gnu17",
    "-pedantic",
    "-Wall",
    "-Werror",
    "-Wextra",
    "-Wpedantic",

    # allow the enure macro to be called without args
    "-Wno-gnu-zero-variadic-macro-arguments",
]

def we_library(name, **kwargs):
    cc_library(
        name = name,
        copts = COPTS,
        strip_include_prefix = "/src",
        **kwargs
    )

TEST_COPTS = [
    "-Wno-strict-prototypes",
    "-fsanitize=address",
    "-fsanitize=undefined",
]

TEST_LOPTS = [
    "-fsanitize=address",
    "-fsanitize=undefined",
]

def we_test(name, deps = [], **kwargs):
    cc_test(
        name = name,
        size = "small",
        copts = COPTS + TEST_COPTS,
        linkopts = TEST_LOPTS,
        deps = deps + ["//third_party/snow:main"],
        **kwargs
    )
