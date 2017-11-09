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

#ifndef IPC_SRC_HOST_IMPL_H_
#define IPC_SRC_HOST_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/task_runner.h"
#include "ipc/deferred.h"
#include "ipc/host.h"
#include "ipc/src/buffered_frame_deserializer.h"
#include "ipc/src/unix_socket.h"

namespace perfetto {
namespace ipc {

class Frame;

class HostImpl : public Host, public UnixSocket::EventListener {
 public:
  HostImpl(const char* socket_name, base::TaskRunner*);
  ~HostImpl() override;

  // Host implementation.
  bool ExposeService(std::unique_ptr<Service>) override;

  // UnixSocket::EventListener implementation.
  void OnNewIncomingConnection(UnixSocket*,
                               std::unique_ptr<UnixSocket>) override;
  void OnDisconnect(UnixSocket*) override;
  void OnDataAvailable(UnixSocket*) override;

  const UnixSocket* sock() const { return sock_.get(); }

 private:
  // Owns the per-client receive buffer (BufferedFrameDeserializer).
  struct ClientConnection {
    ~ClientConnection();
    ClientID id;
    std::unique_ptr<UnixSocket> sock;
    BufferedFrameDeserializer frame_deserializer;
  };
  struct ExposedService {
    ExposedService(ServiceID, const std::string&, std::unique_ptr<Service>);
    ~ExposedService();
    ExposedService(ExposedService&&) noexcept;
    ExposedService& operator=(ExposedService&&);

    ServiceID id;
    std::string name;
    std::unique_ptr<Service> instance;
  };

  HostImpl(const HostImpl&) = delete;
  HostImpl& operator=(const HostImpl&) = delete;

  bool Initialize(const char* socket_name);
  void OnReceivedFrame(ClientConnection*, const Frame&);
  void OnBindService(ClientConnection*, const Frame&);
  void OnInvokeMethod(ClientConnection*, const Frame&);
  void ReplyToMethodInvocation(ClientID, RequestID, AsyncResult<ProtoMessage>);
  const ExposedService* GetServiceByName(const std::string&);

  static void SendFrame(ClientConnection*, const Frame&);

  const char* const socket_name_;
  base::TaskRunner* const task_runner_;
  base::WeakPtrFactory<HostImpl> weak_ptr_factory_;
  std::map<ServiceID, ExposedService> services_;
  std::unique_ptr<UnixSocket> sock_;  // The listening socket.
  std::map<ClientID, std::unique_ptr<ClientConnection>> clients_;
  std::map<UnixSocket*, ClientConnection*> clients_by_socket_;
  ServiceID last_service_id_ = 0;
  ClientID last_client_id_ = 0;
};

}  // namespace ipc
}  // namespace perfetto

#endif  // IPC_SRC_HOST_IMPL_H_
