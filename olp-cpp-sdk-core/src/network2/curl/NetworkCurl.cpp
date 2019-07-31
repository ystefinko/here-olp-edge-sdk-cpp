/*
 * Copyright (C) 2019 HERE Europe B.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 * License-Filename: LICENSE
 */

#include "NetworkCurl.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace olp {
namespace network2 {

namespace impl {

CurlNetwork::CurlNetwork(int connection_pool_size) {
  curl_multi_handle_ = curl_multi_init();
  for (int i = 0; i < connection_pool_size; i++) {
    free_handles_.emplace_back(curl_easy_init());
  }

  pipe(worker_notification_pipe_);
  fcntl(worker_notification_pipe_[0], F_SETFL, O_NONBLOCK);
  fcntl(worker_notification_pipe_[1], F_SETFL, O_NONBLOCK);
  curl_custom_wait_.fd = worker_notification_pipe_[0];
  curl_custom_wait_.events = CURL_WAIT_POLLIN;
  curl_custom_wait_.revents = 0;

  running_.store(true);
  worker_ = std::thread(&CurlNetwork::Run, this);
}

CurlNetwork::~CurlNetwork() {
  running_.store(false);
  NotifyWorkerThread();
  worker_.join();

  close(worker_notification_pipe_[0]);
  close(worker_notification_pipe_[1]);

  for (auto easy_handle : free_handles_) {
    curl_easy_cleanup(easy_handle);
  }
  curl_multi_cleanup(curl_multi_handle_);
}

void CurlNetwork::FlushWorkerNotifications() {
  if (!(curl_custom_wait_.revents & CURL_WAIT_POLLIN)) {
    return;
  }
  char unused;
  while (read(worker_notification_pipe_[0], &unused, sizeof(unused)) > 0)
    ;
  return;
}

size_t curl_write_callback(char* ptr, size_t size, size_t nmemb,
                           void* userdata) {
  RequestContext& context = *reinterpret_cast<RequestContext*>(userdata);
  const auto length = size * nmemb;

  if (context.payload) {
    context.payload->write(ptr, length);
  }

  if (context.data_callback) {
    context.data_callback(reinterpret_cast<std::uint8_t*>(ptr),
                          context.write_cursor, length);
  }

  context.write_cursor += length;
  return length;
};

size_t curl_read_callback(char* buffer, size_t size, size_t nitems,
                          void* userdata) {
  RequestContext& context = *reinterpret_cast<RequestContext*>(userdata);
  auto length = size * nitems;

  auto& request_body = *context.request.GetBody();

  if (context.read_cursor + length > request_body.size()) {
    length = request_body.size() - context.read_cursor;
  }

  auto src_begin = request_body.begin() + context.read_cursor;
  auto src_end = src_begin + length;
  std::copy(src_begin, src_end, buffer);

  context.read_cursor += length;
  return length;
};

size_t curl_header_callback(char* buffer, size_t size, size_t nitems,
                            void* userdata) {
  RequestContext& context = *reinterpret_cast<RequestContext*>(userdata);
  const auto header_length = size * nitems;
  if (!context.header_callback) {
    return header_length;
  }

  const auto header_begin = buffer;
  const auto header_end = buffer + header_length;
  auto colon_char = std::find(header_begin, header_end, ':');
  if (colon_char == header_end) {
    return 0;
  }
  std::string header_name(header_begin, colon_char);

  // skip ": "
  std::advance(colon_char, 2);
  if (colon_char >= header_end) {
    return 0;
  }
  std::string header_value(colon_char, header_end);

  context.header_callback(std::move(header_name), std::move(header_value));
  return header_length;
};

void CurlNetwork::Run() {
  while (running_.load()) {
    CURLMcode rc;

    // Wait either for notification or for network transfer
    int numfds;
    rc = curl_multi_wait(curl_multi_handle_, &curl_custom_wait_, 1, 1000,
                         &numfds);
    // Perform network transfer if it's ready
    int still_running;
    rc = curl_multi_perform(curl_multi_handle_, &still_running);

    ProcessFinishedTransfers();
    HandleCancelledTransfers();
    FlushWorkerNotifications();
    StartNewTransfers();
  };

  for (auto& handle_context_pair : running_requests_) {
    CURL* e_handle = handle_context_pair.first;
    curl_multi_remove_handle(curl_multi_handle_, e_handle);
    curl_easy_reset(e_handle);
    free_handles_.push_back(e_handle);
  }
  running_requests_.clear();
}

void CurlNetwork::HandleCancelledTransfers() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto request_id : cancel_list_) {
    auto it = running_requests_index_.find(request_id);
    if (it != running_requests_index_.end()) {
      // this request is already running
      auto easy_handle = it->second;
      curl_multi_remove_handle(curl_multi_handle_, easy_handle);
      curl_easy_reset(easy_handle);
      running_requests_[easy_handle]->callback(
          NetworkResponse().WithCancelled(true));
      running_requests_.erase(easy_handle);
      free_handles_.push_back(easy_handle);
      continue;
    }

    if (request_queue_.find(request_id) != request_queue_.end()) {
      request_queue_[request_id]->callback(
          NetworkResponse().WithCancelled(true));
      request_queue_.erase(request_id);
    }
  }
}

void CurlNetwork::ProcessFinishedTransfers() {
  std::lock_guard<std::mutex> lock(mutex_);
  int msgs_in_queue;
  CURLMsg* curl_message;
  while ((curl_message = curl_multi_info_read(curl_multi_handle_,
                                              &msgs_in_queue)) != NULL) {
    if (curl_message->msg != CURLMSG_DONE) {
      continue;
    }
    CURL* e_handle = curl_message->easy_handle;
    long response_code;
    curl_easy_getinfo(e_handle, CURLINFO_RESPONSE_CODE, &response_code);
    RequestId request_id = running_requests_[e_handle]->request_id;
    auto response = NetworkResponse()
                        .WithRequestId(request_id)
                        .WithStatus(response_code)
                        .WithCancelled(false);
    running_requests_[e_handle]->callback(std::move(response));
    curl_multi_remove_handle(curl_multi_handle_, e_handle);
    curl_easy_reset(e_handle);
    running_requests_.erase(e_handle);
    running_requests_index_.erase(request_id);
    free_handles_.push_back(e_handle);
  }
}

void CurlNetwork::StartNewTransfers() {
  std::lock_guard<std::mutex> lock(mutex_);

  while (!free_handles_.empty() && !request_queue_.empty()) {
    CURL* e_handle = free_handles_.back();
    free_handles_.pop_back();

    auto request_context = std::move(request_queue_.begin()->second);
    request_queue_.erase(request_queue_.begin());
    request_context->curl_handle = e_handle;
    curl_easy_setopt(e_handle, CURLOPT_URL,
                     request_context->request.GetUrl().c_str());
    if (!request_context->request.GetHeaders().empty()) {
      for (const auto& header : request_context->request.GetHeaders()) {
        const std::string& name = header.first;
        const std::string& value = header.second;
        std::string header_string = name + ": " + value;
        request_context->header_list = curl_slist_append(
            request_context->header_list, header_string.c_str());
      }
      curl_easy_setopt(e_handle, CURLOPT_HTTPHEADER,
                       request_context->header_list);
    }
    curl_easy_setopt(e_handle, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(e_handle, CURLOPT_READFUNCTION, curl_read_callback);
    curl_easy_setopt(e_handle, CURLOPT_HEADERFUNCTION, curl_header_callback);
    curl_easy_setopt(e_handle, CURLOPT_WRITEDATA, request_context.get());
    curl_easy_setopt(e_handle, CURLOPT_READDATA, request_context.get());
    curl_easy_setopt(e_handle, CURLOPT_HEADERDATA, request_context.get());

    const auto& proxy_settings =
        request_context->request.GetSettings().GetProxySettings();

    const auto& proxy_type = proxy_settings.GetType();
    switch (proxy_type) {
      case NetworkProxySettings::Type::HTTP:
        curl_easy_setopt(e_handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
        break;
      case NetworkProxySettings::Type::SOCKS4:
        curl_easy_setopt(e_handle, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
        break;
      case NetworkProxySettings::Type::SOCKS4A:
        curl_easy_setopt(e_handle, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4A);
        break;
      case NetworkProxySettings::Type::SOCKS5:
        curl_easy_setopt(e_handle, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
        break;
      case NetworkProxySettings::Type::SOCKS5_HOSTNAME:
        curl_easy_setopt(e_handle, CURLOPT_PROXYTYPE,
                         CURLPROXY_SOCKS5_HOSTNAME);
        break;
      default:
        break;
    }
    if (proxy_type != NetworkProxySettings::Type::NONE) {
      curl_easy_setopt(e_handle, CURLOPT_PROXY,
                       proxy_settings.GetHostname().c_str());
      curl_easy_setopt(e_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
      std::string auth =
          proxy_settings.GetUsername() + ":" + proxy_settings.GetPassword();
      curl_easy_setopt(e_handle, CURLOPT_PROXYUSERPWD, auth.c_str());
    }
    curl_easy_setopt(
        e_handle, CURLOPT_CONNECTTIMEOUT,
        request_context->request.GetSettings().GetConnectionTimeout());
    curl_easy_setopt(
        e_handle, CURLOPT_TIMEOUT,
        request_context->request.GetSettings().GetTransferTimeout());
    curl_easy_setopt(e_handle, CURLOPT_NOSIGNAL, 1);

    curl_multi_add_handle(curl_multi_handle_, e_handle);
    running_requests_.emplace(e_handle, std::move(request_context));
    running_requests_index_.emplace(request_context->request_id, e_handle);
  }
}

RequestId CurlNetwork::Send(NetworkRequest request,
                            std::shared_ptr<std::ostream> payload,
                            Callback callback, HeaderCallback header_callback,
                            DataCallback data_callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (next_request_id_ == RequestIdConstants::RequestIdMax) {
    next_request_id_ = RequestIdConstants::RequestIdMin;
  } else {
    next_request_id_++;
  }
  std::unique_ptr<RequestContext> request_context(
      new RequestContext{next_request_id_, std::move(request),
                         std::move(payload), std::move(callback),
                         std::move(header_callback), std::move(data_callback)});

  request_queue_.emplace(next_request_id_, std::move(request_context));
  NotifyWorkerThread();
  return next_request_id_;
}

void CurlNetwork::Cancel(RequestId id) {
  std::lock_guard<std::mutex> lock(mutex_);

  cancel_list_.push_back(id);
  NotifyWorkerThread();
}

void CurlNetwork::NotifyWorkerThread() {
  write(worker_notification_pipe_[1], ".", 1);
}

}  // namespace impl
}  // end of namespace network2
}  // end of namespace olp
