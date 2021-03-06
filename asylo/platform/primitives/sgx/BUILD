#
# Copyright 2019 Asylo authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

licenses(["notice"])  # Apache v2.0

load("//asylo/bazel:copts.bzl", "ASYLO_DEFAULT_COPTS")

package(
    default_visibility = ["//asylo:implementation"],
)

# Trusted runtime components for SGX.
cc_library(
    name = "trusted_sgx",
    srcs = [
        "trusted_runtime.cc",
    ],
    copts = ASYLO_DEFAULT_COPTS,
    tags = ["asylo-sgx"],
    deps = [
        "//asylo/platform/primitives:trusted_runtime",
    ],
)

cc_library(
    name = "untrusted_sgx",
    hdrs = ["untrusted_sgx.h"],
    copts = ASYLO_DEFAULT_COPTS,
    deps = [
        "//asylo:enclave_proto_cc",
        "//asylo/platform/primitives",
        "//asylo/platform/primitives:untrusted_primitives",
        "//asylo/util:status",
        "@com_google_absl//absl/strings",
    ],
)
