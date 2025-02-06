load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

COPTS = [
    "-std=gnu17",
    "-pedantic",
    "-Wall",
    "-Werror",
    "-Wextra",
    "-Wpedantic",
    "-Wno-gnu-zero-variadic-macro-arguments",
]

def we_library(name, **kwargs):
    cc_library(
        name = name,
        copts = COPTS,
        strip_include_prefix = "/src",
        **kwargs
    )

def we_test(name, deps = [], **kwargs):
    cc_test(
        name = name,
        size = "small",
        copts = COPTS + ["-Wno-strict-prototypes"],
        deps = deps + ["//third_party/snow:main"],
        **kwargs
    )
