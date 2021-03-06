/*
 *
 * Copyright 2018 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <array>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/debugging/leak_check.h"
#include "absl/memory/memory.h"
#include "asylo/platform/primitives/extent.h"
#include "asylo/platform/primitives/parameter_stack.h"
#include "asylo/platform/primitives/primitive_status.h"
#include "asylo/platform/primitives/test/test_backend.h"
#include "asylo/platform/primitives/test/test_selectors.h"
#include "asylo/platform/primitives/trusted_primitives.h"
#include "asylo/platform/primitives/untrusted_primitives.h"
#include "asylo/platform/primitives/util/dispatch_table.h"
#include "asylo/test/util/status_matchers.h"
#include "asylo/util/status.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;

namespace asylo {
namespace primitives {
namespace {

class PrimitivesTest : public ::testing::Test {
 protected:
  // Loads the enclave. When 'reload' is false, the enclave is indeed loaded,
  // and initialization is expected to happen.
  std::shared_ptr<EnclaveClient> LoadTestEnclaveOrDie(bool reload) {
    auto exit_call_provider = absl::make_unique<DispatchTable>();
    // Register init exit call invoked during test_enclave.cc initialization.
    MockFunction<Status(std::shared_ptr<class EnclaveClient> enclave, void *,
                        UntrustedParameterStack *params)>
        mock_init_handler;
    if (reload) {
      EXPECT_CALL(mock_init_handler, Call(NotNull(), _, _))
          .WillRepeatedly(Return(Status::OkStatus()));
    } else {
      EXPECT_CALL(mock_init_handler, Call(NotNull(), _, _))
          .WillOnce(Return(Status::OkStatus()));
    }
    Status status = exit_call_provider->RegisterExitHandler(
        kUntrustedInit, ExitHandler{mock_init_handler.AsStdFunction()});
    EXPECT_THAT(status, IsOk());
    return test::TestBackend::Get()->LoadTestEnclaveOrDie(
        std::move(exit_call_provider));
  }

  static void TearDownTestCase() {
    // Clean up the backend.
    delete test::TestBackend::Get();
  }
};

// Enter an instance of the test enclave and multiply a number by two, aborting
// on failure.
int32_t MultiplyByTwoOrDie(const std::shared_ptr<EnclaveClient> &client,
                           int32_t value) {
  UntrustedParameterStack params;
  params.Push<int32_t>(value);
  Status status = client->EnclaveCall(kTimesTwoSelector, &params);
  EXPECT_THAT(status, IsOk());
  EXPECT_FALSE(params.empty());
  const int32_t res = params.Pop<int32_t>();
  EXPECT_TRUE(params.empty());
  return res;
}

// Ensure making an invalid call into an enclave returns an appropriate failure
// status.

TEST_F(PrimitivesTest, BadCall) {
  auto client = LoadTestEnclaveOrDie(/*reload=*/false);

  // Enter the enclave with an invalid selector.
  UntrustedParameterStack params;
  params.PushAlloc(4096);
  Status status = client->EnclaveCall(kNotRegisteredSelector, &params);
  EXPECT_THAT(status, Not(IsOk()));

  // Invoke a selector with invalid number of arguments.
  UntrustedParameterStack params_0_arg;
  status = client->EnclaveCall(kTimesTwoSelector, &params_0_arg);
  EXPECT_THAT(status, Not(IsOk()));
  UntrustedParameterStack params_2_arg;
  params_2_arg.PushAlloc(4096);
  params_2_arg.PushAlloc(4096);
  status = client->EnclaveCall(kTimesTwoSelector, &params_2_arg);
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(PrimitivesTest, EnclaveLifetime) {
  // Ensure that an enclave is not closed before its clients leave scope.
  auto client = LoadTestEnclaveOrDie(/*reload=*/false);
  auto first_copy = client;
  auto second_copy = client;

  first_copy.reset();
  EXPECT_FALSE(client->IsClosed());
  second_copy.reset();
  EXPECT_FALSE(client->IsClosed());

  // Ensure that destroying an enclave is immediately reflected in all clients.
  first_copy = client;
  second_copy = client;
  EXPECT_FALSE(first_copy->IsClosed());
  EXPECT_FALSE(second_copy->IsClosed());
  client->Destroy();
  EXPECT_TRUE(client->IsClosed());
  EXPECT_TRUE(first_copy->IsClosed());
  EXPECT_TRUE(second_copy->IsClosed());

  // Ensure that multiple instances of the same enclave are independent.
  auto first_instance = LoadTestEnclaveOrDie(/*reload=*/false);
  auto second_instance = LoadTestEnclaveOrDie(/*reload=*/true);

  EXPECT_FALSE(first_instance->IsClosed());
  EXPECT_THAT(MultiplyByTwoOrDie(first_instance, 1), Eq(2));
  EXPECT_FALSE(second_instance->IsClosed());
  EXPECT_THAT(MultiplyByTwoOrDie(second_instance, 2), Eq(4));

  first_instance->Destroy();
  EXPECT_TRUE(first_instance->IsClosed());
  EXPECT_FALSE(second_instance->IsClosed());
  EXPECT_THAT(MultiplyByTwoOrDie(second_instance, 3), Eq(6));

  second_instance->Destroy();
  EXPECT_TRUE(first_instance->IsClosed());
  EXPECT_TRUE(second_instance->IsClosed());
}

// Test basic enclave load, send message, and destroy.
TEST_F(PrimitivesTest, LoadEnclave) {
  auto client = LoadTestEnclaveOrDie(/*reload=*/false);
  EXPECT_FALSE(client->IsClosed());

  // Make an enclave call.
  EXPECT_THAT(MultiplyByTwoOrDie(client, 1), Eq(2));

  // Close the enclave.
  client->Destroy();
  EXPECT_TRUE(client->IsClosed());

  // Ensure a call to a destroyed enclave fails.
  UntrustedParameterStack params;
  int32_t input = 1;
  params.Push<int32_t>(input);
  Status status = client->EnclaveCall(kTimesTwoSelector, &params);
  EXPECT_THAT(status, Not(IsOk()));
}

// Ensure that an aborted enclave cannot be reentered.
TEST_F(PrimitivesTest, AbortEnclave) {
  // If the enclave is aborted, then the call to the finalizer routine and
  // memory allocated by it will never be freed.
  const std::unique_ptr<absl::LeakCheckDisabler> ignore_leak_check(
      test::TestBackend::Get()->LeaksMemoryOnAbort()
          ? new absl::LeakCheckDisabler()
          : nullptr);

  auto client = LoadTestEnclaveOrDie(/*reload=*/false);
  std::shared_ptr<EnclaveClient> client_copy = client;

  // Make an enclave call.
  EXPECT_THAT(MultiplyByTwoOrDie(client, 1), Eq(2));

  // Abort the enclave.
  UntrustedParameterStack abort_params;
  Status status = client->EnclaveCall(kAbortEnclaveSelector, &abort_params);
  ASSERT_THAT(status, IsOk());

  // Check that we can't enter the enclave again.
  int32_t value = 10;
  UntrustedParameterStack params;
  params.Push<int32_t>(value);
  status = client->EnclaveCall(kTimesTwoSelector, &params);
  EXPECT_THAT(status, Not(IsOk()));

  // Check that we can't enter through a copy of the client.
  status = client_copy->EnclaveCall(kTimesTwoSelector, &params);
  EXPECT_THAT(status, Not(IsOk()));
}

// Test control flow passing in and out of an enclave.
TEST_F(PrimitivesTest, CallChain) {
  auto client = LoadTestEnclaveOrDie(/*reload=*/false);

  auto trusted_fibonacci = [&client](int32_t n) -> int32_t {
    UntrustedParameterStack params;
    *params.PushAlloc<int32_t>() = n;
    Status status = client->EnclaveCall(kTrustedFibonacci, &params);
    EXPECT_THAT(status, IsOk());
    EXPECT_FALSE(params.empty());
    const int32_t res = params.Pop<int32_t>();
    EXPECT_TRUE(params.empty());
    return res;
  };

  // An exit handler to compute a Fibonacci number, calling back into the
  // enclave recursively.
  ExitHandler::Callback fibonacci_handler =
      [&](std::shared_ptr<EnclaveClient> client, void *context,
          UntrustedParameterStack *params) -> Status {
    if (params->empty()) {
      return Status{error::GoogleError::INVALID_ARGUMENT,
                    "TrustedFiboncci called with incorrent argument(s)."};
    }
    const int32_t n = params->Pop<int32_t>();
    if (!params->empty()) {
      return Status{error::GoogleError::INVALID_ARGUMENT,
                    "TrustedFiboncci called with incorrent argument(s)."};
    }
    if (n >= 50) {
      return Status{error::GoogleError::INVALID_ARGUMENT,
                    "UntrustedFiboncci called with invalid argument."};
    }
    *params->PushAlloc<int32_t>() =
        n < 2 ? n : trusted_fibonacci(n - 1) + trusted_fibonacci(n - 2);
    return Status::OkStatus();
  };

  // Register the Fibonacci exit handler.
  Status status = client->exit_call_provider()->RegisterExitHandler(
      kUntrustedFibonacci, ExitHandler{fibonacci_handler});
  ASSERT_THAT(status, IsOk());
  EXPECT_THAT(trusted_fibonacci(20), Eq(6765));
}

// Ensure many threads can attempt enter the enclave simultaneously.
TEST_F(PrimitivesTest, ThreadedTest) {
  constexpr int kNumThreads = 64;
  auto client = LoadTestEnclaveOrDie(/*reload=*/false);
  for (int i = 0; i < 32; i++) {
    std::vector<std::thread> threads;
    for (int j = 0; j < kNumThreads; j++) {
      threads.emplace_back([&client, j]() {
        auto result = MultiplyByTwoOrDie(client, j);
        EXPECT_THAT(result, Eq(2 * j));
      });
    }
    for (auto &thread : threads) {
      thread.join();
    }
  }
}

// Ensure the buffers returned by trusted malloc satisfy
// TrustedPrimitives::IsTrustedExtent().
TEST_F(PrimitivesTest, TrustedMalloc) {
  auto client = LoadTestEnclaveOrDie(/*reload=*/false);
  UntrustedParameterStack params;
  Status status = client->EnclaveCall(kTrustedMallocTest, &params);
  EXPECT_THAT(status, IsOk());
  EXPECT_FALSE(params.empty());
  EXPECT_TRUE(params.Pop<bool>());
  EXPECT_TRUE(params.empty());
}

// Ensure the buffers returned by untrusted alloc do not satisfy
// TrustedPrimitives::IsTrustedExtent().
TEST_F(PrimitivesTest, UnrustedAlloc) {
  auto client = LoadTestEnclaveOrDie(/*reload=*/false);
  UntrustedParameterStack params;
  Status status = client->EnclaveCall(kUntrustedLocalAllocTest, &params);
  EXPECT_THAT(status, IsOk());
  EXPECT_FALSE(params.empty());
  EXPECT_TRUE(params.Pop<bool>());
  EXPECT_TRUE(params.empty());
}

}  // namespace
}  // namespace primitives
}  // namespace asylo

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  ::google::ParseCommandLineFlags(&argc, &argv,
                                  /*remove_flags=*/ true);

  return RUN_ALL_TESTS();
}
