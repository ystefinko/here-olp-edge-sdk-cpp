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

#include <olp/core/cache/CacheSettings.h>
#include <olp/core/cache/KeyValueCache.h>
#include <olp/core/client/ApiError.h>
#include <olp/core/client/ApiResponse.h>
#include <olp/core/client/OlpClientSettings.h>

#include <olp/dataservice/write/DataServiceWriteApi.h>
#include <olp/dataservice/write/FlushEventListener.h>
#include <olp/dataservice/write/FlushSettings.h>
#include <olp/dataservice/write/generated/model/ResponseOk.h>
#include <olp/dataservice/write/generated/model/ResponseOkSingle.h>


namespace olp {
namespace client {
struct Error;
class HRN;
}  // namespace client

namespace dataservice {
namespace write {
class StreamLayerClientImpl;

namespace model {
class PublishDataRequest;
class PublishSdiiRequest;
}  // namespace model

/**
 * @brief Creates an instance of the default cache with provided settings.
 * @param settings Cache settings.
 *
 * @return DefaultCache instance.
 */
std::shared_ptr<cache::KeyValueCache> CreateDefaultCache(
    const cache::CacheSettings& settings = cache::CacheSettings());

using PublishDataResult = olp::dataservice::write::model::ResponseOkSingle;
using PublishDataResponse =
    client::ApiResponse<PublishDataResult, client::ApiError>;
using PublishDataCallback = std::function<void(PublishDataResponse response)>;

using PublishSdiiResult = olp::dataservice::write::model::ResponseOk;
using PublishSdiiResponse =
    client::ApiResponse<PublishSdiiResult, client::ApiError>;
using PublishSdiiCallback = std::function<void(PublishSdiiResponse response)>;

/**
 * @brief Client responsible for writing data into an OLP Stream Layer.
 */
class DATASERVICE_WRITE_API StreamLayerClient {
 public:
  using FlushResponse = std::vector<PublishDataResponse>;
  using FlushCallback = std::function<void(FlushResponse response)>;
  using FlushListener = FlushEventListener<const FlushResponse&>;

  /**
   * @brief StreamLayerClient Constructor.
   * @param catalog OLP HRN specifying the catalog this client will write to.
   * @param settings Client settings used to control behaviour of this
   * StreamLayerClient instance.
   * @param cache Optional KeyValueCache instance used for queuing. If no cache
   * @param flush_settings Optional defines settings that effect cached
   * partitions to be flushed. is provided a default in-memory cache is used.
   */
  StreamLayerClient(
      const client::HRN& catalog, const client::OlpClientSettings& settings,
      const std::shared_ptr<cache::KeyValueCache>& cache = CreateDefaultCache(),
      const FlushSettings& flush_settings = FlushSettings());

  /**
   * @brief Call to publish data into an OLP Stream Layer.
   * @note Content-type for this request will be set implicitly based on the
   * Layer metadata for the target Layer on OLP.
   * @param request PublishDataRequest object representing the parameters for
   * this publishData call.
   *
   * @return A CancellableFuture containing the PublishDataResponse.
   */
  olp::client::CancellableFuture<PublishDataResponse> PublishData(
      const model::PublishDataRequest& request);

  /**
   * @brief Call to publish data into an OLP Stream Layer.
   * @note Content-type for this request will be set implicitly based on the
   * Layer metadata for the target Layer on OLP.
   * @param request PublishDataRequest object representing the parameters for
   * this publishData call.
   * @param callback PublishDataCallback which will be called with the
   * PublishDataResponse when the operation completes.
   *
   * @return A CancellationToken which can be used to cancel the ongoing
   * request.
   */
  olp::client::CancellationToken PublishData(
      const model::PublishDataRequest& request,
      const PublishDataCallback& callback);

  /**
   * @brief Queue a PublishDataRequest to be sent over the wire at a later time.
   * @param request PublishDataRequest object representing the parameters for
   * the call to be made in the future.
   * @return A boost optional which will be boost::none if the queue call was
   * successful, otherwise it will contain a string with error details.
   */
  boost::optional<std::string> Queue(const model::PublishDataRequest& request);

  /**
   * @brief Flush PublishDataRequests which have been queued via the Queue
   * API.
   * @return A CancellableFuture containing the FlushResponse.
   */
  olp::client::CancellableFuture<FlushResponse> Flush();

  /**
   * @brief Flush PublishDataRequests which have been queued via the Queue
   * API.
   * @return A CancellationToken which can be used to cancel the ongoing
   * request.
   */
  olp::client::CancellationToken Flush(const FlushCallback& callback);

  /**
   * @brief Enable the StreamLayerClient auto-flushing mechanism.
   * @param listener Optional FlushEventListener used for optaining metrics and
   * getting events related to auto flushing.
   */
  void Enable(std::shared_ptr<FlushListener> listener = nullptr);

  /**
   * @brief Disable the StreamLayerClient auto-flushing mechanism. Will cancel
   * any ongoing background flush tasks.
   *
   * @return std::future object, which will be set when all pending background
   * flush events have completed or have been cancelled. Ensure that the
   * StreamLayerClient is not destructed prior to this future being set.
   */
  std::future<void> Disable();

  /**
   * @brief Create a listener object of type DefaultFlushEventListener for this
   * API.
   *
   * @return std::shared_ptr< FlushDataListener > object, of appropriate type
   * for this API.
   */
  static std::shared_ptr<FlushListener> DefaultListener();

  /**
   * @brief Call to send a list of SDII messages to an OLP Stream Layer.
   * SDII message data must be in SDII MessageList protobuf format. For more
   * information please see the OLP Sensor Data Ingestion Interface
   * documentation and schemas.
   * @param request PublishSdiiRequest object representing the parameters for
   * this publishSdii call.
   *
   * @return A CancellableFuture containing the PublishSdiiResponse.
   */
  olp::client::CancellableFuture<PublishSdiiResponse> PublishSdii(
      const model::PublishSdiiRequest& request);

  /**
   * @brief Call to send a list of SDII messages to an OLP Stream Layer.
   * SDII message data must be in SDII MessageList protobuf format. For more
   * information please see the OLP Sensor Data Ingestion Interface
   * documentation and schemas.
   * @param request PublishSdiiRequest object representing the parameters for
   * this publishSdii call.
   * @param callback PublishSdiiCallback which will be called with the
   * PublishSdiiResponse when the operation completes.
   *
   * @return A CancellationToken which can be used to cancel the ongoing
   * request.
   */
  olp::client::CancellationToken PublishSdii(
      const model::PublishSdiiRequest& request,
      const PublishSdiiCallback& callback);

 private:
  std::shared_ptr<StreamLayerClientImpl> impl_;
};

}  // namespace write
}  // namespace dataservice
}  // namespace olp
