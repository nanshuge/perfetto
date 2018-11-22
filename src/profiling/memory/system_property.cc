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

#include "src/profiling/memory/system_property.h"

#include "perfetto/base/logging.h"

#include <android-base/properties.h>

namespace perfetto {
namespace profiling {

SystemProperties::Handle::Handle(Handle&& other) {
  system_properties_ = other.system_properties_;
  property_ = std::move(other.property_);
  all_ = other.all_;
  other.system_properties_ = nullptr;
}

SystemProperties::Handle& SystemProperties::Handle::operator=(Handle&& other) {
  system_properties_ = other.system_properties_;
  property_ = std::move(other.property_);
  all_ = other.all_;
  other.system_properties_ = nullptr;
  return *this;
}

SystemProperties::Handle::Handle(SystemProperties* system_properties)
    : system_properties_(system_properties), all_(true) {}

SystemProperties::Handle::Handle(SystemProperties* system_properties,
                                 std::string property)
    : system_properties_(system_properties), property_(std::move(property)) {}

SystemProperties::Handle::~Handle() {
  if (system_properties_) {
    if (all_)
      system_properties_->UnsetAll();
    else
      system_properties_->UnsetProperty(property_);
  }
}

SystemProperties::Handle::operator bool() {
  return system_properties_ != nullptr;
}

SystemProperties::Handle SystemProperties::SetProperty(std::string name) {
  auto it = properties_.find(name);
  if (it == properties_.end()) {
    // Profiling for name is not enabled, enable it.
    if (!android::base::SetProperty("heapprofd.enable." + name, "1"))
      return Handle(nullptr);
    if (properties_.size() == 1 || alls_ == 0) {
      if (!android::base::SetProperty("heapprofd.enable", "1"))
        return Handle(nullptr);
    }
    // Keep this last to ensure we only emplace if enabling was
    // successful.
    properties_.emplace(name, 1);
  } else {
    // Profiling for name is already enabled, increment refcount.
    it->second++;
  }
  return Handle(this, std::move(name));
}

SystemProperties::Handle SystemProperties::SetAll() {
  if (alls_ == 0) {
    // Profiling all is currently not enabled, enable it.
    if (!android::base::SetProperty("heapprofd.enable", "all"))
      return Handle(nullptr);
  }
  // Profiling all is already enabled, increment refcount.
  alls_++;
  return Handle(this);
}

SystemProperties::~SystemProperties() {
  PERFETTO_DCHECK(alls_ == 0 && properties_.empty());
}

void SystemProperties::UnsetProperty(const std::string& name) {
  auto it = properties_.find(name);
  if (it == properties_.end()) {
    PERFETTO_DFATAL("Unsetting unknown property.");
    return;
  }
  if (--(it->second) == 0) {
    // Last reference went away, unset flag for name.
    properties_.erase(it);
    android::base::SetProperty("heapprofd.enable." + name, "");
    if (properties_.empty() && alls_ == 0) {
      android::base::SetProperty("heapprofd.enable", "");
    }
  }
}

void SystemProperties::UnsetAll() {
  if (--alls_ == 0) {
    if (properties_.empty())
      android::base::SetProperty("heapprofd.enable", "");
    else
      android::base::SetProperty("heapprofd.enable", "1");
  }
}

}  // namespace profiling
}  // namespace perfetto
