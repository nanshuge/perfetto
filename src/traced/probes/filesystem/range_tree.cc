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

#include "src/traced/probes/filesystem/range_tree.h"

namespace perfetto {

const std::set<std::string> RangeTree::Get(Inode inode) {
  std::set<std::string> ret;
  auto lower = map_.upper_bound(inode);
  if (lower != map_.begin())
    lower--;
  for (const auto x : lower->second)
    ret.emplace(x->ToString());
  return ret;
}

void RangeTree::Insert(Inode inode, RangeTree::DataType interned) {
  auto lower = map_.rbegin();
  if (!map_.empty()) {
    PERFETTO_CHECK(inode > lower->first);
  }

  if (map_.empty() || !lower->second.Add(interned)) {
    SmallSet<PrefixFinder::Node*, kSetSize> n;
    n.Add(interned);
    map_.emplace(inode, std::move(n));
  }
}
}  // namespace perfetto
