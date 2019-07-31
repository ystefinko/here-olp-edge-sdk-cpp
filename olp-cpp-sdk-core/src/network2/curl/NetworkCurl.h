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

#include "olp/core/network2/Network.h"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "RequestContext.h"

namespace olp {
namespace network2 {
namespace impl {

class CurlNetwork final : public Network {
 public:
  CurlNetwork(int connection_pool_size);
  virtual ~CurlNetwork() override;

  RequestId Send(NetworkRequest request, std::shared_ptr<std::ostream> payload,
                 Callback callback, HeaderCallback header_callback = nullptr,
                 DataCallback data_callback = nullptr) override;
  void Cancel(RequestId id) override;

 private:
  void NotifyWorkerThread();

  // methods used by internal worker thread
  void Run();  /// worker thread starts here
  void FlushWorkerNotifications();
  void StartNewTransfers();
  void HandleCancelledTransfers();
  void ProcessFinishedTransfers();

  std::thread worker_;

  CURLM* curl_multi_handle_;
  std::vector<CURL*> free_handles_;
  curl_waitfd curl_custom_wait_;

  std::map<CURL*, std::unique_ptr<RequestContext>> running_requests_;
  std::map<RequestId, CURL*>
      running_requests_index_;  /// helps to find request context by request id

  /// these members are accessed by both clients and worker thread
  std::mutex mutex_;
  RequestId next_request_id_ = RequestIdConstants::RequestIdMin;
  int worker_notification_pipe_[2];
  std::atomic<bool> running_;
  std::map<RequestId, std::unique_ptr<RequestContext>> request_queue_;
  std::vector<RequestId> cancel_list_;
};

}  // namespace impl
}  // namespace network2
}  // namespace olp
