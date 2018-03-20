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

#include "src/traced/probes/filesystem/prefix_finder.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/string_splitter.h"

namespace perfetto {
std::string PrefixFinder::Node::ToString() {
  if (parent_ != nullptr)
    return parent_->ToString() + "/" + name_;
  return name_;
}

PrefixFinder::PrefixFinder(size_t limit) : limit_(limit) {}

void PrefixFinder::AddPath(std::string path) {
  perfetto::base::StringSplitter s(std::move(path), '/');
  for (size_t i = 0; s.Next(); ++i) {
    char* token = s.cur_token();
    if (i < state_.size()) {
      std::pair<std::string, size_t>& elem = state_[i];
      if (elem.first == token) {
        elem.second++;
      } else {
        if (i == 0 || state_[i - 1].second > limit_) {
          Node* cur = &root_;
          for (auto it = state_.cbegin();
               it != state_.cbegin() + static_cast<ssize_t>(i + 1); it++) {
            std::unique_ptr<Node>& next = cur->children_[it->first];
            if (!next)
              next.reset(new Node(it->first, cur));
            cur = next.get();
          }
        }
        elem.first = token;
        elem.second = 1;
        state_.resize(i + 1);
      }
    } else {
      state_.emplace_back(token, 1);
    }
  }
}

PrefixFinder::Node* PrefixFinder::GetPrefix(std::string path) {
  perfetto::base::StringSplitter s(std::move(path), '/');
  Node* cur = &root_;
  for (size_t i = 0; s.Next(); ++i) {
    char* token = s.cur_token();
    auto it = cur->children_.find(token);
    if (it == cur->children_.end())
      break;
    cur = it->second.get();
    PERFETTO_DCHECK(cur->name_ == token);
  }
  return cur;
}
}  // namespace perfetto
