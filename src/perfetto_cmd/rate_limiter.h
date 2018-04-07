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

#ifndef SRC_PERFETTO_CMD_RATE_LIMITER_H_
#define SRC_PERFETTO_CMD_RATE_LIMITER_H_

#include "perfetto/base/build_config.h"
#include "perfetto/base/time.h"

PERFETTO_COMPILER_WARNINGS_SUPPRESSION_BEGIN()
#include "src/perfetto_cmd/perfetto_cmd_state.pb.h"
PERFETTO_COMPILER_WARNINGS_SUPPRESSION_END()

namespace perfetto {

class RateLimiter {
 public:
  struct Args {
    bool is_dropbox = false;
    bool ignore_guardrails = false;
    base::TimeSeconds current_time = base::TimeSeconds(0);
  };

  RateLimiter();
  virtual ~RateLimiter();

  bool ShouldTrace(const Args& args);
  bool OnTraceDone(const Args& args, bool success, size_t bytes);

  bool ClearState();

  // virtual for testing.
  virtual bool LoadState(PerfettoCmdState* state);

  // virtual for testing.
  virtual bool SaveState(const PerfettoCmdState& state);

  bool StateFileExists();
  virtual std::string GetStateFilePath() const;

 private:
  PerfettoCmdState state_{};
};

}  // namespace perfetto

#endif  // SRC_PERFETTO_CMD_RATE_LIMITER_H_
