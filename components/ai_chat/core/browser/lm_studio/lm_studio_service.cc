/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/ai_chat/core/browser/lm_studio/lm_studio_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "brave/components/ai_chat/core/common/mojom/lm_studio.mojom.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ai_chat {

namespace {

constexpr net::NetworkTrafficAnnotationTag kLmStudioConnectionAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "brave_leo_assistant_lm_studio_connection",
        R"(
        semantics {
          sender: "Brave Leo Assistant"
          description:
            "Check if LM Studio is running on localhost to enable fetching."
          trigger:
            "User accesses Leo Assistant settings with LM Studio fetching."
          data:
            "HTTP request to localhost:1234 to check LM Studio availability."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This feature can be controlled in Leo Assistant settings."
        })");

constexpr net::NetworkTrafficAnnotationTag kLmStudioModelsAnnotation =
    net::DefineNetworkTrafficAnnotation("brave_leo_assistant_lm_studio_models",
                                        R"(
        semantics {
          sender: "Brave Leo Assistant"
          description:
            "Fetch available models from local LM Studio instance for chat."
          trigger:
            "User enables LM Studio fetching in Leo Assistant settings."
          data:
            "HTTP request to localhost:1234/v1/models for models."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This feature can be disabled in Leo Assistant settings."
        })");

constexpr size_t kConnectionCheckMaxSize = 1024;
constexpr size_t kModelListMaxSize = 1024 * 1024;

}  // namespace

LmStudioService::LmStudioService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<LmStudioModelFetcher> model_fetcher)
    : url_loader_factory_(std::move(url_loader_factory)),
      model_fetcher_(std::move(model_fetcher)) {
  if (model_fetcher_) {
    model_fetcher_->SetDelegate(this);
  }
}

LmStudioService::~LmStudioService() = default;

void LmStudioService::Shutdown() {
  model_fetcher_.reset();
}

void LmStudioService::BindReceiver(
    mojo::PendingReceiver<mojom::LmStudioService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LmStudioService::IsConnected(IsConnectedCallback callback) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(mojom::kLmStudioBaseUrl);
  request->method = net::HttpRequestHeaders::kGetMethod;

  auto loader = network::SimpleURLLoader::Create(
      std::move(request), kLmStudioConnectionAnnotation);

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&LmStudioService::OnConnectionCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(loader)),
      kConnectionCheckMaxSize);
}

void LmStudioService::OnConnectionCheckComplete(
    IsConnectedCallback callback,
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::optional<std::string> response) {
  bool connected = loader->ResponseInfo() && loader->ResponseInfo()->headers &&
                   loader->ResponseInfo()->headers->response_code() == 200;

  std::move(callback).Run(connected);
}

void LmStudioService::FetchModels(ModelsCallback callback) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(mojom::kLmStudioListModelsAPIEndpoint);
  request->method = net::HttpRequestHeaders::kGetMethod;

  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 kLmStudioModelsAnnotation);

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&LmStudioService::OnModelsListComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(loader)),
      kModelListMaxSize);
}

void LmStudioService::OnModelsListComplete(
    ModelsCallback callback,
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::optional<std::string> response) {
  if (!response) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(ParseModelsResponse(*response));
}

std::optional<std::vector<std::string>> LmStudioService::ParseModelsResponse(
    const std::string& response_body) {
  std::optional<base::DictValue> json_dict = base::JSONReader::ReadDict(
      response_body, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!json_dict) {
    return std::nullopt;
  }

  const base::ListValue* models_list = json_dict->FindList("data");
  if (!models_list) {
    return std::nullopt;
  }

  std::vector<std::string> models;
  for (const auto& model : *models_list) {
    const base::DictValue* model_dict = model.GetIfDict();
    if (!model_dict) {
      continue;
    }

    const std::string* model_id = model_dict->FindString("id");
    if (!model_id) {
      continue;
    }

    models.push_back(*model_id);
  }

  return models;
}

}  // namespace ai_chat
