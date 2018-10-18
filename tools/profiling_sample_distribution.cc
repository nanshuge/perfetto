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

// Tool that takes in a stream of allocations from stdin and

#include <iostream>
#include <string>

#include <unistd.h>

#include "src/profiling/memory/client.h"
#include "src/profiling/memory/sampler.h"

#include "perfetto/base/logging.h"

namespace perfetto {
namespace {

constexpr uint64_t kDefaultSamplingRate = 128000;

int ProfilingSampleDistributionMain(int argc, char** argv) {
  int opt;
  uint64_t sampling_rate = kDefaultSamplingRate;
  long long times = 1;

  while ((opt = getopt(argc, argv, "t:r:")) != -1) {
    switch (opt) {
      case 'r': {
        char* end;
        long long sampling_rate_arg = strtoll(optarg, &end, 10);
        if (*end != '\0' || *optarg == '\0')
          PERFETTO_FATAL("Invalid sampling rate: %s", optarg);
        PERFETTO_CHECK(sampling_rate > 0);
        sampling_rate = static_cast<uint64_t>(sampling_rate_arg);
        break;
      }
      case 't': {
        char* end;
        times = strtoll(optarg, &end, 10);
        if (*end != '\0' || *optarg == '\0')
          PERFETTO_FATAL("Invalid times: %s", optarg);
        break;
      }
    }
  }

  std::vector<std::pair<std::string, uint64_t>> allocations;

  while (std::cin) {
    std::string callsite;
    uint64_t size;
    std::cin >> callsite;
    if (std::cin.fail()) {
      // Skip trailing newline.
      if (std::cin.eof())
        break;
      PERFETTO_FATAL("Could not read callsite");
    }
    std::cin >> size;
    if (std::cin.fail())
      PERFETTO_FATAL("Could not read size");
    allocations.emplace_back(std::move(callsite), size);
  }

  while (times-- > 0) {
    PThreadKey key(ThreadLocalSamplingData::KeyDestructor);
    // We want to use the same API here that the client uses,
    // which involves TLS. In order to destruct that TLS, we need to spawn a
    // thread because pthread_key_delete does not delete any associated data.
    //
    // Sad times.
    std::thread th([&] {
      if (!key.valid())
        PERFETTO_FATAL("Failed to initialize TLS.");

      std::map<std::string, uint64_t> totals;
      for (const auto& pair : allocations) {
        size_t sample_size =
            SampleSize(key.get(), pair.second, sampling_rate, malloc, free);
        if (sample_size > 0)
          totals[pair.first] += sample_size;
      }

      for (const auto& pair : totals)
        std::cout << pair.first << " " << pair.second << std::endl;
    });
    th.join();
  }

  return 0;
}

}  // namespace
}  // namespace perfetto

int main(int argc, char** argv) {
  return perfetto::ProfilingSampleDistributionMain(argc, argv);
}