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

#include "src/traced/probes/ftrace/ftrace_config_muxer.h"

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "perfetto/base/utils.h"
#include "src/traced/probes/ftrace/atrace_wrapper.h"

namespace perfetto {
namespace {

// trace_clocks in preference order.
constexpr const char* kClocks[] = {"boot", "global", "local"};

constexpr int kDefaultPerCpuBufferSizeKb = 512;    // 512kb
constexpr int kMaxPerCpuBufferSizeKb = 64 * 1024;  // 64mb

std::vector<GroupAndName> difference(const std::set<GroupAndName>& a,
                                     const std::set<GroupAndName>& b) {
  std::vector<GroupAndName> result;
  result.reserve(std::max(b.size(), a.size()));
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                      std::inserter(result, result.begin()));
  return result;
}

void AddEventGroup(const ProtoTranslationTable* table,
                   const std::string& group,
                   std::set<GroupAndName>* to) {
  const std::vector<const Event*>* events = table->GetEventsByGroup(group);
  if (!events)
    return;
  for (const Event* event : *events)
    to->insert(GroupAndName(group, event->name));
}

std::set<GroupAndName> ReadEventsInGroupFromFs(FtraceProcfs* ftrace_procfs,
                                               const std::string& group) {
  std::set<std::string> names =
      ftrace_procfs->GetEventNamesForGroup("events/" + group);
  std::set<GroupAndName> events;
  for (const auto& name : names)
    events.insert(GroupAndName(group, name));
  return events;
}

}  // namespace

std::set<GroupAndName> FtraceConfigMuxer::GetFtraceEvents(
    const FtraceConfig& request,
    const ProtoTranslationTable* table) {
  std::set<GroupAndName> events;
  for (const auto& config_value : request.ftrace_events()) {
    auto group_and_name = GroupAndName(config_value);
    if (group_and_name.name() == "*") {
      events = ReadEventsInGroupFromFs(ftrace_, group_and_name.group());
    } else if (group_and_name.group().empty()) {
      const Event* e = table->GetEvent(group_and_name);
      if (!e) {
        PERFETTO_DLOG(
            "Event doesn't exist: %s. Include the group in the config to allow "
            "the event to be output as a generic event.",
            group_and_name.name().c_str());
        continue;
      }
      events.insert(GroupAndName(e->group, e->name));
    } else {
      events.insert(group_and_name);
    }
  }
  if (RequiresAtrace(request)) {
    events.insert(GroupAndName("ftrace", "print"));

    // Ideally we should keep this code in sync with:
    // platform/frameworks/native/cmds/atrace/atrace.cpp
    // It's not a disaster if they go out of sync, we can always add the ftrace
    // categories manually server side but this is user friendly and reduces the
    // size of the configs.
    for (const std::string& category : request.atrace_categories()) {
      if (category == "gfx") {
        AddEventGroup(table, "mdss", &events);
        AddEventGroup(table, "sde", &events);
        continue;
      }

      if (category == "sched") {
        events.insert(GroupAndName(category, "sched_switch"));
        events.insert(GroupAndName(category, "sched_wakeup"));
        events.insert(GroupAndName(category, "sched_waking"));
        events.insert(GroupAndName(category, "sched_blocked_reason"));
        events.insert(GroupAndName(category, "sched_cpu_hotplug"));
        AddEventGroup(table, "cgroup", &events);
        continue;
      }

      if (category == "irq") {
        AddEventGroup(table, "irq", &events);
        AddEventGroup(table, "ipi", &events);
        continue;
      }

      if (category == "irqoff") {
        events.insert(GroupAndName(category, "irq_enable"));
        events.insert(GroupAndName(category, "irq_disable"));
        continue;
      }

      if (category == "preemptoff") {
        events.insert(GroupAndName(category, "preempt_enable"));
        events.insert(GroupAndName(category, "preempt_disable"));
        continue;
      }

      if (category == "i2c") {
        AddEventGroup(table, "i2c", &events);
        continue;
      }

      if (category == "freq") {
        events.insert(GroupAndName("power", "cpu_frequency"));
        events.insert(GroupAndName("power", "clock_set_rate"));
        events.insert(GroupAndName("power", "clock_disable"));
        events.insert(GroupAndName("power", "clock_enable"));
        events.insert(GroupAndName("clk", "clk_set_rate"));
        events.insert(GroupAndName("clk", "clk_disable"));
        events.insert(GroupAndName("clk", "clk_enable"));
        events.insert(GroupAndName("power", "cpu_frequency_limits"));
        continue;
      }

      if (category == "membus") {
        AddEventGroup(table, "memory_bus", &events);
        continue;
      }

      if (category == "idle") {
        events.insert(GroupAndName("power", "cpu_idle"));
        continue;
      }

      if (category == "disk") {
        events.insert(GroupAndName("f2fs", "f2fs_sync_file_enter"));
        events.insert(GroupAndName("f2fs", "f2fs_sync_file_exit"));
        events.insert(GroupAndName("f2fs", "f2fs_write_begin"));
        events.insert(GroupAndName("f2fs", "f2fs_write_end"));
        events.insert(GroupAndName("ext4", "ext4_da_write_begin"));
        events.insert(GroupAndName("ext4", "ext4_da_write_end"));
        events.insert(GroupAndName("ext4", "ext4_sync_file_enter"));
        events.insert(GroupAndName("ext4", "ext4_sync_file_exit"));
        events.insert(GroupAndName("block", "block_rq_issue"));
        events.insert(GroupAndName("block", "block_rq_complete"));
        continue;
      }

      if (category == "mmc") {
        AddEventGroup(table, "mmc", &events);
        continue;
      }

      if (category == "load") {
        AddEventGroup(table, "cpufreq_interactive", &events);
        continue;
      }

      if (category == "sync") {
        AddEventGroup(table, "sync", &events);
        continue;
      }

      if (category == "workq") {
        AddEventGroup(table, "workqueue", &events);
        continue;
      }

      if (category == "memreclaim") {
        events.insert(GroupAndName("vmscan", "mm_vmscan_direct_reclaim_begin"));
        events.insert(GroupAndName("vmscan", "mm_vmscan_direct_reclaim_end"));
        events.insert(GroupAndName("vmscan", "mm_vmscan_kswapd_wake"));
        events.insert(GroupAndName("vmscan", "mm_vmscan_kswapd_sleep"));
        AddEventGroup(table, "lowmemorykiller", &events);
        continue;
      }

      if (category == "regulators") {
        AddEventGroup(table, "regulator", &events);
        continue;
      }

      if (category == "binder_driver") {
        events.insert(GroupAndName("binder", "binder_transaction"));
        events.insert(GroupAndName("binder", "binder_transaction_received"));
        events.insert(GroupAndName("binder", "binder_set_priority"));
        continue;
      }

      if (category == "binder_lock") {
        events.insert(GroupAndName("binder", "binder_lock"));
        events.insert(GroupAndName("binder", "binder_locked"));
        events.insert(GroupAndName("binder", "binder_unlock"));
        continue;
      }

      if (category == "pagecache") {
        AddEventGroup(table, "pagecache", &events);
        continue;
      }
    }
  }
  return events;
}

// Post-conditions:
// 1. result >= 1 (should have at least one page per CPU)
// 2. result * 4 < kMaxTotalBufferSizeKb
// 3. If input is 0 output is a good default number.
size_t ComputeCpuBufferSizeInPages(size_t requested_buffer_size_kb) {
  if (requested_buffer_size_kb == 0)
    requested_buffer_size_kb = kDefaultPerCpuBufferSizeKb;
  if (requested_buffer_size_kb > kMaxPerCpuBufferSizeKb) {
    PERFETTO_ELOG(
        "The requested ftrace buf size (%zu KB) is too big, capping to %d KB",
        requested_buffer_size_kb, kMaxPerCpuBufferSizeKb);
    requested_buffer_size_kb = kMaxPerCpuBufferSizeKb;
  }

  size_t pages = requested_buffer_size_kb / (base::kPageSize / 1024);
  if (pages == 0)
    return 1;

  return pages;
}

FtraceConfigMuxer::FtraceConfigMuxer(FtraceProcfs* ftrace,
                                     ProtoTranslationTable* table)
    : ftrace_(ftrace), table_(table), current_state_(), configs_() {}
FtraceConfigMuxer::~FtraceConfigMuxer() = default;

FtraceConfigId FtraceConfigMuxer::SetupConfig(const FtraceConfig& request) {
  FtraceConfig actual;
  bool is_ftrace_enabled = ftrace_->IsTracingEnabled();
  if (configs_.empty()) {
    PERFETTO_DCHECK(active_configs_.empty());
    PERFETTO_DCHECK(!current_state_.tracing_on);

    // If someone outside of perfetto is using ftrace give up now.
    if (is_ftrace_enabled)
      return 0;

    // Setup ftrace, without starting it. Setting buffers can be quite slow
    // (up to hundreds of ms).
    SetupClock(request);
    SetupBufferSize(request);
  } else {
    // Did someone turn ftrace off behind our back? If so give up.
    if (!active_configs_.empty() && !is_ftrace_enabled)
      return 0;
  }

  std::set<GroupAndName> events = GetFtraceEvents(request, table_);

  if (RequiresAtrace(request))
    UpdateAtrace(request);

  for (const auto& group_and_name : events) {
    const Event* event = table_->GetOrCreateEvent(group_and_name);
    if (!event) {
      PERFETTO_DLOG("Can't enable %s, event not known",
                    group_and_name.ToString().c_str());
      continue;
    }
    if (current_state_.ftrace_events.count(group_and_name.ToString()) ||
        std::string("ftrace") == event->group) {
      *actual.add_ftrace_events() = group_and_name.ToString();
      continue;
    }
    if (ftrace_->EnableEvent(event->group, event->name)) {
      current_state_.ftrace_events.insert(group_and_name.ToString());
      *actual.add_ftrace_events() = group_and_name.ToString();
    } else {
      PERFETTO_DPLOG("Failed to enable %s.", group_and_name.ToString().c_str());
    }
  }

  FtraceConfigId id = ++last_id_;
  configs_.emplace(id, std::move(actual));
  return id;
}

bool FtraceConfigMuxer::ActivateConfig(FtraceConfigId id) {
  if (!id || configs_.count(id) == 0) {
    PERFETTO_DFATAL("Config not found");
    return false;
  }

  active_configs_.insert(id);
  if (active_configs_.size() > 1) {
    PERFETTO_DCHECK(current_state_.tracing_on);
    return true;  // We are not the first, ftrace is already enabled. All done.
  }

  PERFETTO_DCHECK(!current_state_.tracing_on);
  if (ftrace_->IsTracingEnabled()) {
    // If someone outside of perfetto is using ftrace give up now.
    return false;
  }

  ftrace_->EnableTracing();
  current_state_.tracing_on = true;
  return true;
}

bool FtraceConfigMuxer::RemoveConfig(FtraceConfigId id) {
  if (!id || !configs_.erase(id))
    return false;

  std::set<GroupAndName> expected_ftrace_events;
  for (const auto& id_config : configs_) {
    const FtraceConfig& config = id_config.second;
    expected_ftrace_events.insert(config.ftrace_events().begin(),
                                  config.ftrace_events().end());
  }

  std::vector<GroupAndName> events_to_disable =
      difference(current_state_.ftrace_events, expected_ftrace_events);

  for (auto& group_and_name : events_to_disable) {
    const Event* event = table_->GetEvent(group_and_name);
    if (!event)
      continue;
    if (ftrace_->DisableEvent(event->group, event->name))
      current_state_.ftrace_events.erase(group_and_name);
  }

  // If there aren't any more active configs, disable ftrace.
  auto active_it = active_configs_.find(id);
  if (active_it != active_configs_.end()) {
    PERFETTO_DCHECK(current_state_.tracing_on);
    active_configs_.erase(active_it);
    if (active_configs_.empty()) {
      // This was the last active config, disable ftrace.
      ftrace_->DisableTracing();
      current_state_.tracing_on = false;
    }
  }

  // Even if we don't have any other active configs, we might still have idle
  // configs around. Tear down the rest of the ftrace config only if all
  // configs are removed.
  if (configs_.empty()) {
    ftrace_->SetCpuBufferSizeInPages(0);
    ftrace_->DisableAllEvents();
    ftrace_->ClearTrace();
    if (current_state_.atrace_on)
      DisableAtrace();
  }

  return true;
}

const FtraceConfig* FtraceConfigMuxer::GetConfig(FtraceConfigId id) {
  if (!configs_.count(id))
    return nullptr;
  return &configs_.at(id);
}

void FtraceConfigMuxer::SetupClock(const FtraceConfig&) {
  std::string current_clock = ftrace_->GetClock();
  std::set<std::string> clocks = ftrace_->AvailableClocks();

  for (size_t i = 0; i < base::ArraySize(kClocks); i++) {
    std::string clock = std::string(kClocks[i]);
    if (!clocks.count(clock))
      continue;
    if (current_clock == clock)
      break;
    ftrace_->SetClock(clock);
    break;
  }
}

void FtraceConfigMuxer::SetupBufferSize(const FtraceConfig& request) {
  size_t pages = ComputeCpuBufferSizeInPages(request.buffer_size_kb());
  ftrace_->SetCpuBufferSizeInPages(pages);
  current_state_.cpu_buffer_size_pages = pages;
}

void FtraceConfigMuxer::UpdateAtrace(const FtraceConfig& request) {
  PERFETTO_DLOG("Update atrace config...");

  std::vector<std::string> args;
  args.push_back("atrace");  // argv0 for exec()
  args.push_back("--async_start");
  args.push_back("--only_userspace");
  for (const auto& category : request.atrace_categories())
    args.push_back(category);
  if (!request.atrace_apps().empty()) {
    args.push_back("-a");
    std::string arg = "";
    for (const auto& app : request.atrace_apps()) {
      arg += app;
      if (app != request.atrace_apps().back())
        arg += ",";
    }
    args.push_back(arg);
  }

  if (RunAtrace(args))
    current_state_.atrace_on = true;

  PERFETTO_DLOG("...done");
}

void FtraceConfigMuxer::DisableAtrace() {
  PERFETTO_DCHECK(current_state_.atrace_on);

  PERFETTO_DLOG("Stop atrace...");

  if (RunAtrace({"atrace", "--async_stop", "--only_userspace"}))
    current_state_.atrace_on = false;

  PERFETTO_DLOG("...done");
}

}  // namespace perfetto
