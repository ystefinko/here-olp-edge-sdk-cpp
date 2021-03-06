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

#include "PlatformApi.h"

#include <olp/core/client/ApiError.h>
#include <olp/core/client/OlpClient.h>
#include <olp/core/network/HttpResponse.h>

// clang-format off
#include "generated/parser/ApiParser.h"
#include <olp/core/generated/parser/JsonParser.h>
// clang-format on

namespace olp {
namespace dataservice {
namespace write {
client::CancellationToken PlatformApi::GetApis(
    std::shared_ptr<client::OlpClient> client, const std::string& service,
    const std::string& serviceVersion, const ApisCallback& apisCallback) {
  std::multimap<std::string, std::string> headerParams;
  headerParams.insert(std::make_pair("Accept", "application/json"));
  std::multimap<std::string, std::string> queryParams;
  std::multimap<std::string, std::string> formParams;

  std::string platformUrl = "/platform/apis/" + service + "/" + serviceVersion;

  client::NetworkAsyncCallback callback = [apisCallback](
                                              network::HttpResponse response) {
    if (response.status != 200) {
      apisCallback(
          ApisResponse(client::ApiError(response.status, response.response)));
    } else {
      // parse the services
      // TODO catch any exception and return as Error
      apisCallback(ApisResponse(parser::parse<model::Apis>(response.response)));
    }
  };

  return client->CallApi(platformUrl, "GET", queryParams, headerParams,
                         formParams, nullptr, "", callback);
}

}  // namespace write
}  // namespace dataservice
}  // namespace olp
