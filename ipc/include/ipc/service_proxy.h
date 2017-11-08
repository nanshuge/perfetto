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

#ifndef IPC_INCLUDE_IPC_SERVICE_PROXY_H_
#define IPC_INCLUDE_IPC_SERVICE_PROXY_H_

#include "ipc/basic_types.h"

#include <assert.h>

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "base/weak_ptr.h"
#include "ipc/deferred.h"

namespace perfetto {
namespace ipc {

class Client;
class ServiceDescriptor;

// The base class for the client-side autogenerated stubs that forward method
// invocations to the host. All the methods of this class are meant to be called
// only by the autogenerated code.
class ServiceProxy {
 public:
  class EventListener {
   public:
    virtual ~EventListener() = default;

    // Called once the ServiceProxy has succesffully connected to the host.
    // From this point it is possible to send IPC reuests to the host.
    virtual void OnConnect() {}

    // Called if the connection fails or drops after being established.
    virtual void OnConnectionFailed() {}
  };

  ServiceProxy();
  virtual ~ServiceProxy();

  // Guarantees that no callback will happen after this object has been
  // destroyed. The caller has to guarantee that the |event_listener| stays
  // alive at least as long as the ServiceProxy instance.
  void set_event_listener(EventListener* event_listener) {
    event_listener_ = event_listener;
  }
  EventListener* event_listener() const { return event_listener_; }

  bool connected() const { return service_id_ != 0; }

  void InitializeBinding(base::WeakPtr<Client>,
                         ServiceID,
                         std::map<std::string, MethodID>);

  // Called by the IPC methods in the autogenerated classes.
  void BeginInvoke(const std::string& method_name,
                   const ProtoMessage& request,
                   Deferred<ProtoMessage> reply);

  // Called by ClientImpl.
  // |reply_args| == nullptr means request failure.
  void EndInvoke(RequestID,
                 std::unique_ptr<ProtoMessage> reply_arg,
                 bool has_more);

  // Implemented by the autogenerated class.
  virtual const ServiceDescriptor& GetDescriptor() = 0;

 private:
  base::WeakPtr<Client> client_;
  ServiceID service_id_ = 0;
  std::map<std::string, MethodID> remote_method_ids_;
  std::map<RequestID, Deferred<ProtoMessage>> pending_callbacks_;
  base::WeakPtrFactory<ServiceProxy> weak_ptr_factory_;
  EventListener* event_listener_;
};

}  // namespace ipc
}  // namespace perfetto

#endif  // IPC_INCLUDE_IPC_SERVICE_PROXY_H_
