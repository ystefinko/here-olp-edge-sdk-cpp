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

#pragma once

#include <string>

#include <olp/core/CoreApi.h>
#include <olp/core/network/Network.h>

namespace olp {
namespace network {

/**
 * @brief Response from a REST request.
 */
class CORE_API HttpResponse {
 public:
  HttpResponse() = default;
  HttpResponse(int status, std::string response = std::string())
      : status(status), response(std::move(response)) {}

  /**
   * @brief status Status of the HTTP request.
   * @see olp::network::Network::ErrorCode for negative status numbers
   * @see olp::network::HttpStatusCode for positive status numbers
   */
  int status{olp::network::Network::ErrorCode::UnknownError};
  std::string response;
};

}  // namespace network
}  // namespace olp
