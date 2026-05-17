/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ai_chat/lm_studio/lm_studio_service_factory.h"

#include "base/no_destructor.h"
#include "brave/browser/ai_chat/model_service_factory.h"
#include "brave/components/ai_chat/core/browser/lm_studio/lm_studio_model_fetcher.h"
#include "brave/components/ai_chat/core/browser/lm_studio/lm_studio_service.h"
#include "brave/components/ai_chat/core/common/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace ai_chat {

// static
LmStudioServiceFactory* LmStudioServiceFactory::GetInstance() {
  static base::NoDestructor<LmStudioServiceFactory> instance;
  return instance.get();
}

// static
LmStudioService* LmStudioServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<LmStudioService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileSelections LmStudioServiceFactory::CreateProfileSelections() {
  if (!features::IsAIChatEnabled()) {
    return ProfileSelections::BuildNoProfilesSelected();
  }
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOriginalOnly)
      .Build();
}

LmStudioServiceFactory::LmStudioServiceFactory()
    : ProfileKeyedServiceFactory("LmStudioServiceFactory",
                                 CreateProfileSelections()) {
  DependsOn(ModelServiceFactory::GetInstance());
}

LmStudioServiceFactory::~LmStudioServiceFactory() = default;

std::unique_ptr<KeyedService>
LmStudioServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();

  auto* model_service = ModelServiceFactory::GetForBrowserContext(context);
  auto* prefs = user_prefs::UserPrefs::Get(context);

  std::unique_ptr<LmStudioModelFetcher> model_fetcher;
  if (model_service && prefs) {
    model_fetcher = std::make_unique<LmStudioModelFetcher>(
        *model_service, prefs, /*delegate=*/nullptr);
  }

  return std::make_unique<LmStudioService>(url_loader_factory,
                                           std::move(model_fetcher));
}

}  // namespace ai_chat
