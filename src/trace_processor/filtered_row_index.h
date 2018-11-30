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

#ifndef SRC_TRACE_PROCESSOR_FILTERED_ROW_INDEX_H_
#define SRC_TRACE_PROCESSOR_FILTERED_ROW_INDEX_H_

#include <stdint.h>
#include <algorithm>
#include <vector>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_processor {

class FilteredRowIndex {
 public:
  enum Mode {
    kAllRows = 1,
    kBitVector = 2,
  };

  FilteredRowIndex(uint32_t start_row, uint32_t end_row);

  template <typename Predicate>
  void FilterRows(Predicate fn) {
    switch (mode_) {
      case Mode::kAllRows:
        FilterAllRows(fn);
        break;
      case Mode::kBitVector:
        FilterBitVector(fn);
        break;
    }
  }

  std::vector<bool> ReleaseBitVector() {
    auto vector = std::move(row_filter_);
    row_filter_.clear();
    mode_ = Mode::kAllRows;
    return vector;
  }

  Mode mode() const { return mode_; }

 private:
  template <typename Predicate>
  void FilterAllRows(Predicate fn) {
    row_filter_.resize(end_row_ - start_row_, true);
    for (uint32_t i = start_row_; i < end_row_; i++) {
      row_filter_[i - start_row_] = fn(i);
    }
    // Update the mode to use the bitvector.
    mode_ = Mode::kBitVector;
  }

  template <typename Predicate>
  void FilterBitVector(Predicate fn) {
    auto b = row_filter_.begin();
    auto e = row_filter_.end();
    using std::find;
    for (auto it = find(b, e, true); it != e; it = find(it + 1, e, true)) {
      auto filter_idx = static_cast<uint32_t>(std::distance(b, it));
      *it = fn(start_row_ + filter_idx);
    }
  }

  Mode mode_;
  uint32_t start_row_;
  uint32_t end_row_;
  std::vector<bool> row_filter_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_FILTERED_ROW_INDEX_H_