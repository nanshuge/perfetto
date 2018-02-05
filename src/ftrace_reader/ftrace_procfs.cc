/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "ftrace_procfs.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>
#include <sstream>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/utils.h"

namespace perfetto {
namespace {

// TODO(b/72148971): Unify with other constants.
static constexpr size_t kPageSize = 4096;

// Reading /trace produces human readable trace output.
// Writing to this file clears all trace buffers for all CPUS.

// Writing to /trace_marker file injects an event into the trace buffer.

// Reading /tracing_on returns 1/0 if tracing is enabled/disabled.
// Writing 1/0 to this file enables/disables tracing.
// Disabling tracing with this file prevents further writes but
// does not clear the buffer.

char ReadOneCharFromFile(const std::string& path) {
  base::ScopedFile fd = base::OpenFile(path.c_str(), O_RDONLY);
  PERFETTO_CHECK(fd);
  char result = '\0';
  ssize_t bytes = PERFETTO_EINTR(read(fd.get(), &result, 1));
  PERFETTO_CHECK(bytes == 1 || bytes == -1);
  return result;
}

std::string ReadFileIntoString(const std::string& path) {
  std::ifstream fin(path, std::ios::in);
  if (!fin) {
    PERFETTO_DLOG("Could not read '%s'", path.c_str());
    return "";
  }

  std::string str;
  // You can't seek or stat the procfs files on Android.
  // The vast majority (884/886) of format files are under 4k.
  str.reserve(4096);
  str.assign(std::istreambuf_iterator<char>(fin),
             std::istreambuf_iterator<char>());

  return str;
}

}  // namespace

// static
std::unique_ptr<FtraceProcfs> FtraceProcfs::Create(const std::string& root) {
  if (!CheckRootPath(root)) {
    return nullptr;
  }
  return std::unique_ptr<FtraceProcfs>(new FtraceProcfs(root));
}

FtraceProcfs::FtraceProcfs(const std::string& root) : root_(root) {}
FtraceProcfs::~FtraceProcfs() = default;

bool FtraceProcfs::EnableEvent(const std::string& group,
                               const std::string& name) {
  std::string path = root_ + "events/" + group + "/" + name + "/enable";
  return WriteToFile(path, "1");
}

bool FtraceProcfs::DisableEvent(const std::string& group,
                                const std::string& name) {
  std::string path = root_ + "events/" + group + "/" + name + "/enable";
  return WriteToFile(path, "0");
}

bool FtraceProcfs::DisableAllEvents() {
  std::string path = root_ + "events/enable";
  return WriteToFile(path, "0");
}

std::string FtraceProcfs::ReadEventFormat(const std::string& group,
                                          const std::string& name) const {
  std::string path = root_ + "events/" + group + "/" + name + "/format";
  return ReadFileIntoString(path);
}

std::string FtraceProcfs::ReadAvailableEvents() const {
  std::string path = root_ + "available_events";
  return ReadFileIntoString(path);
}

size_t FtraceProcfs::NumberOfCpus() const {
  static size_t num_cpus = sysconf(_SC_NPROCESSORS_CONF);
  return num_cpus;
}

void FtraceProcfs::ClearTrace() {
  std::string path = root_ + "trace";
  base::ScopedFile fd = base::OpenFile(path.c_str(), O_WRONLY | O_TRUNC);
  PERFETTO_CHECK(fd);  // Could not clear.
}

bool FtraceProcfs::WriteTraceMarker(const std::string& str) {
  std::string path = root_ + "trace_marker";
  return WriteToFile(path, str);
}

bool FtraceProcfs::SetCpuBufferSizeInPages(size_t pages) {
  if (pages * kPageSize > 1 * 1024 * 1024 * 1024) {
    PERFETTO_ELOG("Tried to set the per CPU buffer size to more than 1gb.");
    return false;
  }
  std::string path = root_ + "buffer_size_kb";
  return WriteNumberToFile(path, pages * (kPageSize / 1024ul));
}

bool FtraceProcfs::EnableTracing() {
  std::string path = root_ + "tracing_on";
  return WriteToFile(path, "1");
}

bool FtraceProcfs::DisableTracing() {
  std::string path = root_ + "tracing_on";
  return WriteToFile(path, "0");
}

bool FtraceProcfs::IsTracingEnabled() {
  std::string path = root_ + "tracing_on";
  return ReadOneCharFromFile(path) == '1';
}

bool FtraceProcfs::SetClock(const std::string& clock_name) {
  std::string path = root_ + "trace_clock";
  return WriteToFile(path, clock_name);
}

std::string FtraceProcfs::GetClock() {
  std::string path = root_ + "trace_clock";
  std::string s = ReadFileIntoString(path);

  size_t start = s.find("[");
  if (start == std::string::npos)
    return "";

  size_t end = s.find("]", start);
  if (end == std::string::npos)
    return "";

  return s.substr(start + 1, end - start - 1);
}

std::set<std::string> FtraceProcfs::AvailableClocks() {
  std::string path = root_ + "trace_clock";
  std::string s = ReadFileIntoString(path);
  std::set<std::string> names;

  size_t start = 0;
  size_t end = 0;

  while ((end = s.find(" ", start)) != std::string::npos) {
    std::string name = s.substr(start, end - start);

    if (name[0] == '[')
      name = name.substr(1, name.size() - 2);

    names.insert(name);

    start = end + 1;
  }

  return names;
}

bool FtraceProcfs::WriteNumberToFile(const std::string& path, size_t value) {
  // 2^65 requires 20 digits to write.
  char buf[21];
  int res = snprintf(buf, 21, "%zu", value);
  if (res < 0 || res >= 21)
    return false;
  return WriteToFile(path, std::string(buf));
}

bool FtraceProcfs::WriteToFile(const std::string& path,
                               const std::string& str) {
  base::ScopedFile fd = base::OpenFile(path.c_str(), O_WRONLY);
  if (!fd)
    return false;
  ssize_t written = PERFETTO_EINTR(write(fd.get(), str.c_str(), str.length()));
  ssize_t length = static_cast<ssize_t>(str.length());
  // This should either fail or write fully.
  PERFETTO_CHECK(written == length || written == -1);
  return written == length;
}

base::ScopedFile FtraceProcfs::OpenPipeForCpu(size_t cpu) {
  std::string path =
      root_ + "per_cpu/cpu" + std::to_string(cpu) + "/trace_pipe_raw";
  return base::OpenFile(path.c_str(), O_RDONLY | O_NONBLOCK);
}

// static
bool FtraceProcfs::CheckRootPath(const std::string& root) {
  base::ScopedFile fd = base::OpenFile(root + "trace", O_RDONLY);
  return static_cast<bool>(fd);
}

}  // namespace perfetto
