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

#include "src/profiling/memory/shared_ring_buffer.h"

#include <array>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

#include "gtest/gtest.h"
#include "perfetto/base/optional.h"

namespace perfetto {
namespace profiling {
namespace {

std::string ToString(const SharedRingBuffer::BufferAndSize& buf_and_size) {
  return std::string(reinterpret_cast<const char*>(&buf_and_size.first[0]),
                     buf_and_size.second);
}

void StructuredTest(SharedRingBuffer* wr, SharedRingBuffer* rd) {
  ASSERT_TRUE(wr);
  ASSERT_TRUE(wr->is_valid());
  ASSERT_TRUE(wr->size() == rd->size());
  const size_t buf_size = wr->size();

  // Test small writes.
  ASSERT_TRUE(wr->Write("foo", 4));
  ASSERT_TRUE(wr->Write("bar", 4));

  auto buf_and_size = rd->Read();
  ASSERT_EQ(buf_and_size.second, 4);
  ASSERT_STREQ(reinterpret_cast<const char*>(&buf_and_size.first[0]), "foo");

  ASSERT_EQ(buf_and_size.second, 4);
  buf_and_size = rd->Read();
  ASSERT_STREQ(reinterpret_cast<const char*>(&buf_and_size.first[0]), "bar");

  for (int i = 0; i < 3; i++) {
    buf_and_size = rd->Read();
    ASSERT_EQ(buf_and_size.first, nullptr);
    ASSERT_EQ(buf_and_size.second, 0);
  }

  // Test extremely large writes (fill the buffer)
  for (int i = 0; i < 3; i++) {
    // Write precisely |buf_size| bytes (minus the size header itself).
    std::string data(buf_size - sizeof(uint32_t), '.' + static_cast<char>(i));
    ASSERT_TRUE(wr->Write(data.data(), data.size()));
    ASSERT_FALSE(wr->Write(data.data(), data.size()));
    ASSERT_FALSE(wr->Write("?", 1));

    // And read it back
    buf_and_size = rd->Read();
    ASSERT_EQ(ToString(buf_and_size), data);
  }

  // Test large writes that wrap.
  std::string data(buf_size / 4 * 3 - sizeof(uint32_t), '!');
  ASSERT_TRUE(wr->Write(data.data(), data.size()));
  ASSERT_FALSE(wr->Write(data.data(), data.size()));
  buf_and_size = rd->Read();
  ASSERT_EQ(ToString(buf_and_size), data);

  data = std::string(base::kPageSize - sizeof(uint32_t), '#');
  for (int i = 0; i < 4; i++)
    ASSERT_TRUE(wr->Write(data.data(), data.size()));

  for (int i = 0; i < 4; i++) {
    buf_and_size = rd->Read();
    ASSERT_EQ(buf_and_size.second, data.size());
    ASSERT_EQ(ToString(buf_and_size), data);
  }

  // Test misaligned writes.
  ASSERT_TRUE(wr->Write("1", 1));
  ASSERT_TRUE(wr->Write("22", 2));
  ASSERT_TRUE(wr->Write("333", 3));
  ASSERT_TRUE(wr->Write("55555", 5));
  ASSERT_TRUE(wr->Write("7777777", 7));
  buf_and_size = rd->Read();
  ASSERT_EQ(ToString(buf_and_size), "1");
  buf_and_size = rd->Read();
  ASSERT_EQ(ToString(buf_and_size), "22");
  buf_and_size = rd->Read();
  ASSERT_EQ(ToString(buf_and_size), "333");
  buf_and_size = rd->Read();
  ASSERT_EQ(ToString(buf_and_size), "55555");
  buf_and_size = rd->Read();
  ASSERT_EQ(ToString(buf_and_size), "7777777");
}

TEST(SharedRingBufferTest, SingleThreadSameInstance) {
  constexpr auto kBufSize = base::kPageSize * 4;
  base::Optional<SharedRingBuffer> buf = SharedRingBuffer::Create(kBufSize);
  StructuredTest(&*buf, &*buf);
}

TEST(SharedRingBufferTest, SingleThreadAttach) {
  constexpr auto kBufSize = base::kPageSize * 4;
  base::Optional<SharedRingBuffer> buf1 = SharedRingBuffer::Create(kBufSize);
  base::Optional<SharedRingBuffer> buf2 =
      SharedRingBuffer::Attach(base::ScopedFile(dup(buf1->fd())));
  StructuredTest(&*buf1, &*buf2);
}

TEST(SharedRingBufferTest, MultiThreadingTest) {
  // TODO got corrupted header with * 128 on mac.  // DNS
  constexpr auto kBufSize = base::kPageSize * 128;
  SharedRingBuffer rd = *SharedRingBuffer::Create(kBufSize);
  SharedRingBuffer wr =
      *SharedRingBuffer::Attach(base::ScopedFile(dup(rd.fd())));

  std::mutex mutex;
  std::unordered_map<std::string, int64_t> expected_contents;
  std::atomic<bool> writers_enabled{false};

  auto writer_thread_fn = [&wr, &expected_contents, &mutex,
                           &writers_enabled](size_t thread_id) {
    while (!writers_enabled.load()) {
    }
    std::minstd_rand0 rnd_engine(static_cast<uint32_t>(thread_id));
    std::uniform_int_distribution<size_t> dist(1, base::kPageSize * 4);
    for (int i = 0; i < 10000; i++) {
      size_t size = dist(rnd_engine);
      std::string data(size, '0' + static_cast<char>(thread_id));
      if (wr.Write(data.data(), data.size())) {
        std::lock_guard<std::mutex> lock(mutex);
        expected_contents[std::move(data)]++;
      }
    }
  };

  auto reader_thread_fn = [&rd, &expected_contents, &mutex, &writers_enabled] {
    for (;;) {
      auto buf_and_size = rd.Read();
      if (!buf_and_size.first) {
        if (!writers_enabled.load()) {
          // Failing to read after the writers are done means that there is no
          // data left in the ring buffer.
          return;
        } else {
          continue;
        }
      }
      ASSERT_GT(buf_and_size.second, 0);
      std::string data = ToString(buf_and_size);
      std::lock_guard<std::mutex> lock(mutex);
      expected_contents[std::move(data)]--;
    }
  };

  constexpr size_t kNumWriterThreads = 4;
  constexpr size_t kNumReaderThreads = 4;
  std::array<std::thread, kNumWriterThreads> writer_threads;
  for (size_t i = 0; i < kNumWriterThreads; i++)
    writer_threads[i] = std::thread(writer_thread_fn, i);

  writers_enabled.store(true);

  std::array<std::thread, kNumReaderThreads> reader_threads;
  for (size_t i = 0; i < kNumReaderThreads; i++)
    reader_threads[i] = std::thread(reader_thread_fn);

  for (size_t i = 0; i < kNumWriterThreads; i++)
    writer_threads[i].join();

  writers_enabled.store(false);

  for (size_t i = 0; i < kNumReaderThreads; i++)
    reader_threads[i].join();

  std::lock_guard<std::mutex> lock(mutex);
  for (const auto& kv : expected_contents) {
    EXPECT_EQ(kv.second, 0) << kv.first << " = " << kv.second;
  }
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
