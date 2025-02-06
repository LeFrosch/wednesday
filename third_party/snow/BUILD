load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "lib",
    testonly = 1,
    hdrs = ["snow.h"],
    defines = ["SNOW_ENABLED"],
    strip_include_prefix = ".",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "main",
    testonly = 1,
    srcs = ["main.c"],
    visibility = ["//visibility:public"],
    deps = [":lib"],
)
