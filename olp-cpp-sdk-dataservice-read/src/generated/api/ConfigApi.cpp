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

#include "ConfigApi.h"

#include <olp/core/client/OlpClient.h>
#include <olp/core/network/HttpResponse.h>
// clang-format off
#include "generated/parser/CatalogParser.h"
#include <olp/core/generated/parser/JsonParser.h>
// clang-format on

#include <map>

namespace olp {
namespace dataservice {
namespace read {
using namespace olp::client;

CancellationToken ConfigApi::GetCatalog(
    const OlpClient& client, const std::string& catalogHrn,
    boost::optional<std::string> billingTag,
    const CatalogCallback& catalogCallback) {
  std::multimap<std::string, std::string> headerParams;
  headerParams.insert(std::make_pair("Accept", "application/json"));
  std::multimap<std::string, std::string> queryParams;
  if (billingTag) {
    queryParams.insert(std::make_pair("billingTag", *billingTag));
  }
  std::multimap<std::string, std::string> formParams;

  std::string catalogUri = "/catalogs/" + catalogHrn;

  NetworkAsyncCallback callback = [catalogCallback](
                                      network::HttpResponse response) {
    if (response.status != 200) {
      catalogCallback(client::ApiError(response.status, response.response));
    } else {
      catalogCallback(olp::parser::parse<model::Catalog>(response.response));
    }
  };

  return client.CallApi(catalogUri, "GET", queryParams, headerParams,
                        formParams, nullptr, "", callback);
}

}  // namespace read

}  // namespace dataservice

}  // namespace olp
