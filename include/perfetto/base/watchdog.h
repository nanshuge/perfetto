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

#ifndef INCLUDE_PERFETTO_BASE_WATCHDOG_H_
#define INCLUDE_PERFETTO_BASE_WATCHDOG_H_

#include "perfetto/base/build_config.h"

#if (PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||    \
     PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)) && \
    !PERFETTO_BUILDFLAG(PERFETTO_CHROMIUM_BUILD)
#include "perfetto/base/watchdog_posix.h"
#else
#include "perfetto/base/watchdog_noop.h"
#endif

#endif  // INCLUDE_PERFETTO_BASE_WATCHDOG_H_
