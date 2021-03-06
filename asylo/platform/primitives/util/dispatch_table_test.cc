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

#include "asylo/platform/primitives/util/dispatch_table.h"

#include <cstdlib>
#include <memory>
#include <random>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "asylo/platform/primitives/parameter_stack.h"
#include "asylo/platform/primitives/untrusted_primitives.h"
#include "asylo/test/util/status_matchers.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::MockFunction;

namespace asylo {
namespace primitives {
namespace {

class MockedEnclaveClient : public EnclaveClient {
 public:
  using MockExitHandlerCallback =
      MockFunction<Status(std::shared_ptr<class EnclaveClient> enclave, void *,
                          UntrustedParameterStack *params)>;

  MockedEnclaveClient() : EnclaveClient(absl::make_unique<DispatchTable>()) {}

  // Virtual methods not used in this test.
  bool IsClosed() const override { return false; }
  void Destroy() override {}
  Status EnclaveCallInternal(uint64_t selector,
                             UntrustedParameterStack *params) override {
    return Status::OkStatus();
  }
};

TEST(DispatchTableTest, HandlersRegistration) {
  DispatchTable dispatch_table;
  MockedEnclaveClient::MockExitHandlerCallback callback;
  ASSERT_THAT(dispatch_table.RegisterExitHandler(
                  0, ExitHandler{callback.AsStdFunction()}),
              IsOk());
  ASSERT_THAT(dispatch_table.RegisterExitHandler(
                  10, ExitHandler{callback.AsStdFunction()}),
              IsOk());
  ASSERT_THAT(dispatch_table.RegisterExitHandler(
                  0, ExitHandler{callback.AsStdFunction()}),
              StatusIs(error::GoogleError::ALREADY_EXISTS));
  ASSERT_THAT(dispatch_table.RegisterExitHandler(
                  10, ExitHandler{callback.AsStdFunction()}),
              StatusIs(error::GoogleError::ALREADY_EXISTS));
  ASSERT_THAT(dispatch_table.RegisterExitHandler(
                  20, ExitHandler{callback.AsStdFunction()}),
              IsOk());
}

TEST(DispatchTableTest, HandlersInvocation) {
  const auto client = std::make_shared<MockedEnclaveClient>();
  MockedEnclaveClient::MockExitHandlerCallback callbacks[3];
  EXPECT_CALL(callbacks[0], Call(Eq(client), _, _)).Times(2);
  EXPECT_CALL(callbacks[1], Call(Eq(client), _, _)).Times(1);
  EXPECT_CALL(callbacks[2], Call(Eq(client), _, _)).Times(0);
  ASSERT_THAT(client->exit_call_provider()->RegisterExitHandler(
                  0, ExitHandler{callbacks[0].AsStdFunction()}),
              IsOk());
  ASSERT_THAT(client->exit_call_provider()->RegisterExitHandler(
                  10, ExitHandler{callbacks[1].AsStdFunction()}),
              IsOk());
  ASSERT_THAT(client->exit_call_provider()->RegisterExitHandler(
                  20, ExitHandler{callbacks[2].AsStdFunction()}),
              IsOk());
  UntrustedParameterStack params;
  EXPECT_THAT(
      client->exit_call_provider()->InvokeExitHandler(0, &params, client.get()),
      IsOk());
  EXPECT_THAT(client->exit_call_provider()->InvokeExitHandler(10, &params,
                                                              client.get()),
              IsOk());
  EXPECT_THAT(
      client->exit_call_provider()->InvokeExitHandler(0, &params, client.get()),
      IsOk());
  EXPECT_THAT(client->exit_call_provider()->InvokeExitHandler(30, &params,
                                                              client.get()),
              StatusIs(error::GoogleError::OUT_OF_RANGE));
}

TEST(DispatchTableTest, HandlersInMultipleThreads) {
  const size_t kThreads = 64;
  const size_t kCount = 256;
  const auto client = std::make_shared<MockedEnclaveClient>();
  MockedEnclaveClient::MockExitHandlerCallback callbacks[kThreads];
  for (size_t i = 0; i < kThreads; ++i) {
    EXPECT_CALL(callbacks[i], Call(Eq(client), _, _)).Times(kCount);
  }
  std::vector<std::unique_ptr<std::thread>> threads;
  threads.reserve(kThreads);
  for (size_t i = 0; i < kThreads; ++i) {
    threads.emplace_back(
        absl::make_unique<std::thread>([i, &client, &callbacks] {
          std::mt19937 rand_engine;
          std::uniform_int_distribution<uint64_t> rand_gen(10, 100);
          absl::SleepFor(absl::Milliseconds(rand_gen(rand_engine)));
          ASSERT_THAT(client->exit_call_provider()->RegisterExitHandler(
                          i, ExitHandler{callbacks[i].AsStdFunction()}),
                      IsOk());
          UntrustedParameterStack params;
          for (size_t c = 0; c < kCount; ++c) {
            absl::SleepFor(absl::Milliseconds(rand_gen(rand_engine)));
            EXPECT_THAT(client->exit_call_provider()->InvokeExitHandler(
                            i, &params, client.get()),
                        IsOk());
          }
        }));
  }
  for (auto &thread : threads) {
    thread->join();
  }
}

}  // namespace
}  // namespace primitives
}  // namespace asylo
