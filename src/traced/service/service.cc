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

#include "perfetto/base/unix_task_runner.h"
#include "perfetto/traced/traced.h"
#include "perfetto/tracing/ipc/service_ipc_host.h"

namespace perfetto {

int ServiceMain(int argc, char** argv) {
  base::UnixTaskRunner task_runner;
  std::unique_ptr<ServiceIPCHost> svc;
  svc = ServiceIPCHost::CreateInstance(&task_runner);
  if (PERFETTO_PRODUCER_SOCK_NAME[0] != '@')
    unlink(PERFETTO_PRODUCER_SOCK_NAME);
  if (PERFETTO_PRODUCER_SOCK_NAME[0] != '@')
    unlink(PERFETTO_CONSUMER_SOCK_NAME);
  svc->Start(PERFETTO_PRODUCER_SOCK_NAME, PERFETTO_CONSUMER_SOCK_NAME);
  PERFETTO_ILOG("Started traced, listening on %s %s",
                PERFETTO_PRODUCER_SOCK_NAME, PERFETTO_CONSUMER_SOCK_NAME);
  task_runner.Run();
  return 0;
}

}  // namespace perfetto
