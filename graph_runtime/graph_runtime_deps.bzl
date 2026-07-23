load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _bazel_skylib():
    http_archive(
        name = "bazel_skylib",
        urls = ["https://github.com/bazelbuild/bazel-skylib/archive/refs/tags/1.6.1.tar.gz"],
        sha256 = "aede1b60709ac12b3461ee0bb3fa097b58a86fbfdb88ef7e9f90424a69043167",
        strip_prefix = "bazel-skylib-1.6.1",
    )

def _nlohmann_json():
    http_archive(
        name = "nlohmann_json",
        sha256 = "a22461d13119ac5c78f205d3df1db13403e58ce1bb1794edc9313677313f4a9d",
        urls = ["https://github.com/nlohmann/json/releases/download/v3.11.3/include.zip"],
        build_file = "//third_party/nlohmann_json:BUILD.bazel",
    )

def _abseil_cpp():
    http_archive(
        name = "com_google_absl",
        sha256 = "733726b8c3a6d39a4120d7e45ea8b41a434cdacde401cba500f14236c49b39dc",
        strip_prefix = "abseil-cpp-20240116.2",
        urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20240116.2.tar.gz"],
    )

def _googletest():
    http_archive(
        name = "com_google_googletest",
        sha256 = "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7",
        strip_prefix = "googletest-1.14.0",
        urls = ["https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"],
    )

def graph_runtime_setup():
    if not native.existing_rule("bazel_skylib"):
        _bazel_skylib()
    if not native.existing_rule("nlohmann_json"):
        _nlohmann_json()
    if not native.existing_rule("com_google_absl"):
        _abseil_cpp()
    if not native.existing_rule("com_google_googletest"):
        _googletest()
