load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

COPTS = ["-std=gnu17", "-pedantic", "-Wall", "-Werror", "-Wextra", "-Wpedantic"]

def we_library(name, **kwargs):
    cc_library(
        name = name,
        copts = COPTS,
        **kwargs,
    )

def we_test(name, deps = [], **kwargs):
    cc_test(
        name = name,
        size = "small",
        copts = COPTS + ["-Wno-strict-prototypes"],
        deps = deps + ["//third_party/snow:main"],
        **kwargs,
    )
