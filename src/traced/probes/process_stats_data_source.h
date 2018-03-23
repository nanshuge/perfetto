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

#ifndef SRC_TRACED_PROBES_PROCESS_STATS_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_PROCESS_STATS_DATA_SOURCE_H_

#include <memory>
#include <set>
#include <vector>

#include "perfetto/base/weak_ptr.h"
#include "perfetto/tracing/core/basic_types.h"
#include "perfetto/tracing/core/trace_writer.h"

namespace perfetto {

class ProcessStatsDataSource {
 public:
  ProcessStatsDataSource(TracingSessionID, std::unique_ptr<TraceWriter> writer);
  ~ProcessStatsDataSource();

  TracingSessionID session_id() const { return session_id_; }
  base::WeakPtr<ProcessStatsDataSource> GetWeakPtr() const;
  void WriteAllProcesses();
  void OnPids(const std::vector<int32_t>& pids);

 private:
  static void WriteProcess(int32_t pid, TraceWriter* writer);

  ProcessStatsDataSource(const ProcessStatsDataSource&) = delete;
  ProcessStatsDataSource& operator=(const ProcessStatsDataSource&) = delete;

  const TracingSessionID session_id_;
  std::unique_ptr<TraceWriter> writer_;
  std::set<int32_t> seen_pids_;
  base::WeakPtrFactory<ProcessStatsDataSource> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_PROCESS_STATS_DATA_SOURCE_H_
