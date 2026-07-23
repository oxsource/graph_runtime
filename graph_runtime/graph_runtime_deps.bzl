load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _nlohmann_json():
    http_archive(
        name = "nlohmann_json",
        sha256 = "5daca6ca216495edf89d167f808d1d03c8a6389f8f8c94a22f3084b9980f59b3",
        urls = ["https://github.com/nlohmann/json/releases/download/v3.11.3/include.zip"],
        build_file = "//third_party/nlohmann_json:BUILD.bazel",
    )

def _abseil_cpp():
    http_archive(
        name = "com_google_absl",
        sha256 = "5366d1e4f7aa4a77c4aa1d8fdbe0e8b87e4b98a74343e84ed243c88d61a6b3c",
        strip_prefix = "abseil-cpp-20240116.2",
        urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20240116.2.tar.gz"],
    )

def _googletest():
    http_archive(
        name = "com_google_googletest",
        sha256 = "8ad598c73b64e74e1a0f5e0e5e2e777a8c0f8c3f0a0e0a0e0a0e0a0e0a0e0a0e",
        strip_prefix = "googletest-1.14.0",
        urls = ["https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"],
    )

def graph_runtime_setup():
    if not native.existing_rule("nlohmann_json"):
        _nlohmann_json()
    if not native.existing_rule("com_google_absl"):
        _abseil_cpp()
    if not native.existing_rule("com_google_googletest"):
        _googletest()
