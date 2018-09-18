/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/profiling/memory/sampler.h"

#include "gtest/gtest.h"

#include <thread>

namespace perfetto {
namespace {

TEST(SamplerTest, TestLarge) {
  pthread_key_t key;
  ASSERT_EQ(pthread_key_create(&key, nullptr), 0);
  EXPECT_EQ(ShouldSample(key, 1024, 512), 1);
  pthread_key_delete(key);
}

TEST(SamplerTest, TestSmall) {
  pthread_key_t key;
  ASSERT_EQ(pthread_key_create(&key, nullptr), 0);
  // As we initialize interval_to_next_sample_ with 0, the first sample
  // should always get sampled.
  EXPECT_EQ(ShouldSample(key, 1, 512), 1);
  pthread_key_delete(key);
}

TEST(SamplerTest, TestSmallFromThread) {
  pthread_key_t key;
  ASSERT_EQ(pthread_key_create(&key, nullptr), 0);
  std::thread th([key] {
    // As we initialize interval_to_next_sample_ with 0, the first sample
    // should always get sampled.
    EXPECT_EQ(ShouldSample(key, 1, 512), 1);
  });
  th.join();
  pthread_key_delete(key);
}

}  // namespace
}  // namespace perfetto
