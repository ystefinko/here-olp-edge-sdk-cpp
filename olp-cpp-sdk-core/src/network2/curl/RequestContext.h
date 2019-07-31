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

namespace olp {
namespace network2 {
namespace impl {

struct RequestContext {
  RequestContext(RequestId id, NetworkRequest request,
                 std::shared_ptr<std::ostream> payload,
                 Network::Callback callback,
                 Network::HeaderCallback header_callback,
                 Network::DataCallback data_callback)
      : request_id(id),
        request(std::move(request)),
        payload(std::move(payload)),
        callback(std::move(callback)),
        header_callback(std::move(header_callback)),
        data_callback(std::move(data_callback)) {}

  // fields filled in Send() method
  RequestId request_id;
  NetworkRequest request;
  std::shared_ptr<std::ostream> payload;
  Network::Callback callback;
  Network::HeaderCallback header_callback;
  Network::DataCallback data_callback;

  // fields used by internal worker thread
  CURL* curl_handle = nullptr;
  curl_slist* header_list = nullptr;
  size_t read_cursor = 0;
  size_t write_cursor = 0;
};

}  // namespace impl
}  // namespace network2
}  // namespace olp
