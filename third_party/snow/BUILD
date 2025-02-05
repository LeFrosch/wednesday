load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "lib",
    hdrs = ["snow.h"],
    strip_include_prefix = ".",
    defines = ["SNOW_ENABLED"],
    visibility = ["//visibility:public"],
    testonly = 1,
)

cc_library(
    name = "main",
    srcs = ["main.c"],
    deps = [":lib"],
    visibility = ["//visibility:public"],
    testonly = 1,
)