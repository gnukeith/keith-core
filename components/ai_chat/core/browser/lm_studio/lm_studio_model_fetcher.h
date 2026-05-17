/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_AI_CHAT_CORE_BROWSER_LM_STUDIO_LM_STUDIO_MODEL_FETCHER_H_
#define BRAVE_COMPONENTS_AI_CHAT_CORE_BROWSER_LM_STUDIO_LM_STUDIO_MODEL_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace ai_chat {

class ModelService;

// Manages fetching models from LM Studio to the AI Chat ModelService.
class LmStudioModelFetcher {
 public:
  class Delegate {
   public:
    using ModelsCallback =
        base::OnceCallback<void(std::optional<std::vector<std::string>>)>;

    virtual ~Delegate() = default;
    virtual void FetchModels(ModelsCallback callback) = 0;
  };

  LmStudioModelFetcher(ModelService& model_service,
                       PrefService* prefs,
                       Delegate* delegate);
  ~LmStudioModelFetcher();

  LmStudioModelFetcher(const LmStudioModelFetcher&) = delete;
  LmStudioModelFetcher& operator=(const LmStudioModelFetcher&) = delete;

  void SetDelegate(Delegate* delegate);

 private:
  friend class LmStudioModelFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(LmStudioModelFetcherTest, FetchModelsAddsNewModels);
  FRIEND_TEST_ALL_PREFIXES(LmStudioModelFetcherTest,
                           FetchModelsRemovesObsoleteModels);
  FRIEND_TEST_ALL_PREFIXES(LmStudioModelFetcherTest,
                           FetchModelsHandlesEmptyResponse);
  FRIEND_TEST_ALL_PREFIXES(LmStudioModelFetcherTest,
                           FetchModelsHandlesInvalidJSON);

  void OnLmStudioFetchEnabledChanged();
  void FetchModels();
  void OnModelsFetched(std::optional<std::vector<std::string>> models);

  const raw_ref<ModelService> model_service_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<Delegate> delegate_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<LmStudioModelFetcher> weak_ptr_factory_{this};
};

}  // namespace ai_chat

#endif  // BRAVE_COMPONENTS_AI_CHAT_CORE_BROWSER_LM_STUDIO_LM_STUDIO_MODEL_FETCHER_H_
