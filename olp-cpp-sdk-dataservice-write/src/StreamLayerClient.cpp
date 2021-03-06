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

#include <olp/dataservice/write/StreamLayerClient.h>

#include <olp/core/cache/DefaultCache.h>

#include "StreamLayerClientImpl.h"

namespace olp {
namespace dataservice {
namespace write {

std::shared_ptr<cache::KeyValueCache> CreateDefaultCache(
    const cache::CacheSettings& settings) {
  auto cache = std::make_shared<cache::DefaultCache>(settings);
  cache->Open();
  return std::move(cache);
}

StreamLayerClient::StreamLayerClient(
    const client::HRN& catalog, const client::OlpClientSettings& settings,
    const std::shared_ptr<cache::KeyValueCache>& cache,
    const FlushSettings& flush_settings)
    : impl_(std::make_shared<StreamLayerClientImpl>(catalog, settings, cache,
                                                    flush_settings)) {}

olp::client::CancellableFuture<PublishDataResponse>
StreamLayerClient::PublishData(const model::PublishDataRequest& request) {
  return impl_->PublishData(request);
}

olp::client::CancellationToken StreamLayerClient::PublishData(
    const model::PublishDataRequest& request,
    const PublishDataCallback& callback) {
  return impl_->PublishData(request, callback);
}

boost::optional<std::string> StreamLayerClient::Queue(
    const model::PublishDataRequest& request) {
  return impl_->Queue(request);
}

olp::client::CancellableFuture<StreamLayerClient::FlushResponse>
StreamLayerClient::Flush() {
  return impl_->Flush();
}

olp::client::CancellationToken StreamLayerClient::Flush(
    const FlushCallback& callback) {
  return impl_->Flush(callback);
}

void StreamLayerClient::Enable(std::shared_ptr<FlushListener> listener) {
  impl_->Enable(listener);
}

std::future<void> StreamLayerClient::Disable() { return impl_->Disable(); }

std::shared_ptr<StreamLayerClient::FlushListener>
StreamLayerClient::DefaultListener() {
  return std::make_shared<DefaultFlushEventListener<const FlushResponse&> >();
}

olp::client::CancellableFuture<PublishSdiiResponse>
StreamLayerClient::PublishSdii(const model::PublishSdiiRequest& request) {
  return impl_->PublishSdii(request);
}

olp::client::CancellationToken StreamLayerClient::PublishSdii(
    const model::PublishSdiiRequest& request,
    const PublishSdiiCallback& callback) {
  return impl_->PublishSdii(request, callback);
}

}  // namespace write
}  // namespace dataservice
}  // namespace olp
