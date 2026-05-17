/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_AI_CHAT_CORE_BROWSER_LM_STUDIO_LM_STUDIO_SERVICE_H_
#define BRAVE_COMPONENTS_AI_CHAT_CORE_BROWSER_LM_STUDIO_LM_STUDIO_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "brave/components/ai_chat/core/browser/lm_studio/lm_studio_model_fetcher.h"
#include "brave/components/ai_chat/core/common/mojom/lm_studio.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace ai_chat {

// Handles network communication with a local LM Studio instance.
class LmStudioService : public KeyedService,
                        public mojom::LmStudioService,
                        public LmStudioModelFetcher::Delegate {
 public:
  using ModelsCallback = LmStudioModelFetcher::Delegate::ModelsCallback;

  LmStudioService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<LmStudioModelFetcher> model_fetcher);
  ~LmStudioService() override;

  LmStudioService(const LmStudioService&) = delete;
  LmStudioService& operator=(const LmStudioService&) = delete;

  void BindReceiver(mojo::PendingReceiver<mojom::LmStudioService> receiver);

  void Shutdown() override;

  void IsConnected(IsConnectedCallback callback) override;

  void FetchModels(ModelsCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LmStudioServiceTest, ParseModelsResponse_Valid);
  FRIEND_TEST_ALL_PREFIXES(LmStudioServiceTest,
                           ParseModelsResponse_InvalidJSON);
  FRIEND_TEST_ALL_PREFIXES(LmStudioServiceTest,
                           ParseModelsResponse_MissingDataKey);
  FRIEND_TEST_ALL_PREFIXES(LmStudioServiceTest, ParseModelsResponse_EmptyData);
  FRIEND_TEST_ALL_PREFIXES(LmStudioServiceTest,
                           ParseModelsResponse_InvalidModelStructure);

  void OnConnectionCheckComplete(
      IsConnectedCallback callback,
      std::unique_ptr<network::SimpleURLLoader> loader,
      std::optional<std::string> response);

  void OnModelsListComplete(ModelsCallback callback,
                            std::unique_ptr<network::SimpleURLLoader> loader,
                            std::optional<std::string> response);

  std::optional<std::vector<std::string>> ParseModelsResponse(
      const std::string& response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  mojo::ReceiverSet<mojom::LmStudioService> receivers_;
  std::unique_ptr<LmStudioModelFetcher> model_fetcher_;
  base::WeakPtrFactory<LmStudioService> weak_ptr_factory_{this};
};

}  // namespace ai_chat

#endif  // BRAVE_COMPONENTS_AI_CHAT_CORE_BROWSER_LM_STUDIO_LM_STUDIO_SERVICE_H_
