/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/ai_chat/core/browser/lm_studio/lm_studio_model_fetcher.h"

#include <set>
#include <string>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "brave/components/ai_chat/core/browser/model_service.h"
#include "brave/components/ai_chat/core/common/constants.h"
#include "brave/components/ai_chat/core/common/mojom/lm_studio.mojom.h"
#include "brave/components/ai_chat/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace ai_chat {

namespace {

constexpr uint32_t kDefaultContextSize = 8192;

}  // namespace

LmStudioModelFetcher::LmStudioModelFetcher(ModelService& model_service,
                                           PrefService* prefs,
                                           Delegate* delegate)
    : model_service_(model_service), prefs_(prefs), delegate_(delegate) {
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      prefs::kBraveAIChatLmStudioFetchEnabled,
      base::BindRepeating(
          &LmStudioModelFetcher::OnLmStudioFetchEnabledChanged,
          weak_ptr_factory_.GetWeakPtr()));

  if (prefs_->GetBoolean(prefs::kBraveAIChatLmStudioFetchEnabled)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LmStudioModelFetcher::FetchModels,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

LmStudioModelFetcher::~LmStudioModelFetcher() = default;

void LmStudioModelFetcher::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void LmStudioModelFetcher::OnLmStudioFetchEnabledChanged() {
  if (prefs_->GetBoolean(prefs::kBraveAIChatLmStudioFetchEnabled)) {
    FetchModels();
  }
}

void LmStudioModelFetcher::FetchModels() {
  if (!delegate_) {
    return;
  }
  delegate_->FetchModels(base::BindOnce(&LmStudioModelFetcher::OnModelsFetched,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void LmStudioModelFetcher::OnModelsFetched(
    std::optional<std::vector<std::string>> models) {
  if (!models) {
    return;
  }

  std::set<std::string> existing_lm_studio_model_names;
  for (const auto& existing_model : model_service_->GetCustomModels()) {
    if (existing_model->options &&
        existing_model->options->is_custom_model_options() &&
        existing_model->options->get_custom_model_options() &&
        existing_model->options->get_custom_model_options()
            ->endpoint.is_valid() &&
        existing_model->options->get_custom_model_options()->endpoint.spec() ==
            mojom::kLmStudioEndpoint) {
      existing_lm_studio_model_names.insert(
          existing_model->options->get_custom_model_options()
              ->model_request_name);
    }
  }

  std::set<std::string> current_lm_studio_models;
  for (const auto& model_name : *models) {
    current_lm_studio_models.insert(model_name);
  }

  model_service_->MaybeDeleteCustomModels(base::BindRepeating(
      [](const std::set<std::string>& current_models,
         const base::DictValue& model_dict) {
        const std::string* endpoint_str =
            model_dict.FindString(kCustomModelItemEndpointUrlKey);
        const std::string* model_name =
            model_dict.FindString(kCustomModelItemModelKey);

        return endpoint_str && model_name &&
               GURL(*endpoint_str) == GURL(mojom::kLmStudioEndpoint) &&
               !current_models.contains(*model_name);
      },
      current_lm_studio_models));

  for (const auto& model_name : *models) {
    if (existing_lm_studio_model_names.contains(model_name)) {
      continue;
    }

    auto custom_model = mojom::Model::New();
    custom_model->key = "";
    custom_model->display_name = model_name;
    custom_model->vision_support = false;
    custom_model->supports_tools = false;
    custom_model->is_suggested_model = false;

    auto custom_options = mojom::CustomModelOptions::New();
    custom_options->model_request_name = model_name;
    custom_options->endpoint = GURL(mojom::kLmStudioEndpoint);
    custom_options->api_key = "";
    custom_options->context_size = kDefaultContextSize;

    custom_model->options =
        mojom::ModelOptions::NewCustomModelOptions(std::move(custom_options));

    model_service_->AddCustomModel(std::move(custom_model));
  }
}

}  // namespace ai_chat
