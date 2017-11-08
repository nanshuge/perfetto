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

#include "ipc/src/client_impl.h"

#include <inttypes.h>

#include "base/task_runner.h"
#include "base/utils.h"
#include "ipc/service_descriptor.h"
#include "ipc/service_proxy.h"

// TODO(primiano): Add ThreadChecker everywhere.

namespace perfetto {
namespace ipc {

// static
std::unique_ptr<Client> Client::CreateInstance(const char* socket_name,
                                               base::TaskRunner* task_runner) {
  std::unique_ptr<Client> client(new ClientImpl(socket_name, task_runner));
  return client;
}

ClientImpl::ClientImpl(const char* socket_name, base::TaskRunner* task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  sock_ = UnixSocket::Connect(socket_name, this, task_runner);
}

ClientImpl::~ClientImpl() {
  OnDisconnect(nullptr);
}

void ClientImpl::BindService(base::WeakPtr<ServiceProxy> service_proxy) {
  if (!service_proxy)
    return;
  RequestID request_id = ++last_request_id_;
  Frame frame;
  frame.set_request_id(request_id);
  Frame::BindService* req = frame.mutable_msg_bind_service();
  const std::string& service_name = service_proxy->GetDescriptor().service_name;
  req->set_service_name(service_name);
  if (!SendFrame(frame)) {
    PERFETTO_DLOG("BindService(%s) failed", service_name.c_str());
    service_proxy->event_listener()->OnConnectionFailed();
  }
  QueuedRequest qr;
  qr.type = Frame::kMsgBindService;
  qr.request_id = request_id;
  qr.service_proxy = service_proxy;
  queued_requests_.emplace(request_id, std::move(qr));
}

void ClientImpl::UnbindService(ServiceID service_id) {
  service_bindings_.erase(service_id);
}

RequestID ClientImpl::BeginInvoke(ServiceID service_id,
                                  const std::string& method_name,
                                  MethodID remote_method_id,
                                  const ProtoMessage& method_args,
                                  base::WeakPtr<ServiceProxy> service_proxy) {
  std::string args_proto;
  RequestID request_id = ++last_request_id_;
  Frame frame;
  frame.set_request_id(request_id);
  Frame::InvokeMethod* req = frame.mutable_msg_invoke_method();
  req->set_service_id(service_id);
  req->set_method_id(remote_method_id);
  bool did_serialize = method_args.SerializeToString(&args_proto);
  req->set_args_proto(args_proto);
  if (!did_serialize || !SendFrame(frame)) {
    return 0;
  }
  QueuedRequest qr;
  qr.type = Frame::kMsgInvokeMethod;
  qr.request_id = request_id;
  qr.method_name = method_name;
  qr.service_proxy = service_proxy;
  queued_requests_.emplace(request_id, std::move(qr));
  return request_id;
}

bool ClientImpl::SendFrame(const Frame& frame) {
  // Serialize the frame into protobuf, add the size header, and send it.
  uint32_t payload_len = static_cast<uint32_t>(frame.ByteSize());
  static constexpr size_t kHeaderSize = sizeof(uint32_t);
  std::unique_ptr<char[]> buf(new char[kHeaderSize + payload_len]);
  if (!frame.SerializeToArray(buf.get() + kHeaderSize, payload_len)) {
    PERFETTO_DCHECK(false);
    return false;
  }
  memcpy(buf.get(), base::AssumeLittleEndian(&payload_len), kHeaderSize);

  // TODO(primiano): remember that this is doing non-blocking I/O. What if the
  // socket buffer is full? Maybe we just want to drop this on the floor? Or
  // maybe throttle the send and PostTask the reply later?
  return sock_->Send(buf.get(), kHeaderSize + payload_len);
}

void ClientImpl::OnConnect(UnixSocket*, bool connected) {}

void ClientImpl::OnDisconnect(UnixSocket*) {
  for (auto it : service_bindings_) {
    base::WeakPtr<ServiceProxy>& service_proxy = it.second;
    if (service_proxy)
      service_proxy->event_listener()->OnConnectionFailed();
  }
  service_bindings_.clear();
}

void ClientImpl::OnDataAvailable(UnixSocket*) {
  size_t rsize;
  do {
    std::pair<char*, size_t> buf = frame_deserializer_.BeginRecv();
    rsize = sock_->Receive(buf.first, buf.second);
    if (!frame_deserializer_.EndRecv(rsize)) {
      // The endpoint tried to send a frame that is way too large.
      return sock_->Shutdown();  // In turn will trigger an OnDisconnect().
    }
  } while (rsize > 0);

  while (std::unique_ptr<Frame> frame = frame_deserializer_.PopNextFrame())
    OnFrameReceived(*frame);
}

void ClientImpl::OnFrameReceived(const Frame& frame) {
  auto queued_requests_it = queued_requests_.find(frame.request_id());
  if (queued_requests_it == queued_requests_.end()) {
    PERFETTO_DLOG("OnFrameReceived() unknown req %" PRIu64, frame.request_id());
    return;
  }
  QueuedRequest req = std::move(queued_requests_it->second);
  queued_requests_.erase(queued_requests_it);

  if (req.type == Frame::kMsgBindService &&
      frame.msg_case() == Frame::kMsgBindServiceReply) {
    return OnBindServiceReply(std::move(req), frame.msg_bind_service_reply());
  }
  if (req.type == Frame::kMsgInvokeMethod &&
      frame.msg_case() == Frame::kMsgInvokeMethodReply) {
    return OnInvokeMethodReply(std::move(req), frame.msg_invoke_method_reply());
  }

  PERFETTO_DLOG(
      "We requestes msg_type=%d but received msg_type=%d in reply to "
      "request_id=%" PRIu64,
      req.type, frame.msg_case(), frame.request_id());
}

void ClientImpl::OnBindServiceReply(QueuedRequest req,
                                    const Frame::BindServiceReply& reply) {
  base::WeakPtr<ServiceProxy>& service_proxy = req.service_proxy;
  if (!service_proxy)
    return;
  if (!reply.success()) {
    PERFETTO_DLOG("Failed BindService(%s)",
                  service_proxy->GetDescriptor().service_name.c_str());
    return service_proxy->event_listener()->OnConnectionFailed();
  }

  // Build the method [name] -> [remote_id] map.
  std::map<std::string, MethodID> methods;
  for (const auto& method : reply.methods()) {
    if (method.name().empty() || method.id() <= 0) {
      PERFETTO_DLOG("OnBindServiceReply() invalid method \"%s\" -> %" PRIu32,
                    method.name().c_str(), method.id());
      continue;
    }
    methods[method.name()] = method.id();
  }
  service_proxy->InitializeBinding(weak_ptr_factory_.GetWeakPtr(),
                                   reply.service_id(), std::move(methods));
  service_bindings_[reply.service_id()] = service_proxy;
  service_proxy->event_listener()->OnConnect();
}

void ClientImpl::OnInvokeMethodReply(QueuedRequest req,
                                     const Frame::InvokeMethodReply& reply) {
  base::WeakPtr<ServiceProxy>& service_proxy = req.service_proxy;
  if (!service_proxy)
    return;
  std::unique_ptr<ProtoMessage> decoded_reply;
  if (reply.success()) {
    // TODO this could be optimized, stop doing method name string lookups.
    for (const auto& method : service_proxy->GetDescriptor().methods) {
      if (req.method_name == method.name) {
        decoded_reply = method.reply_proto_decoder(reply.reply_proto());
        break;
      }
    }
  }
  service_proxy->EndInvoke(req.request_id, std::move(decoded_reply),
                           reply.has_more());
}

ClientImpl::QueuedRequest::QueuedRequest() = default;

}  // namespace ipc
}  // namespace perfetto
