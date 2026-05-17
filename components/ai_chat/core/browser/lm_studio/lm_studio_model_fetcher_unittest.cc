/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/ai_chat/core/browser/lm_studio/lm_studio_model_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "brave/components/ai_chat/core/browser/model_service.h"
#include "brave/components/ai_chat/core/common/mojom/lm_studio.mojom.h"
#include "brave/components/ai_chat/core/common/pref_names.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_chat {

namespace {

using ::testing::_;

class MockDelegate : public LmStudioModelFetcher::Delegate {
 public:
  MOCK_METHOD(void, FetchModels, (ModelsCallback), (override));
};

size_t CountLmStudioModels(ModelService* model_service) {
  size_t count = 0;
  for (const auto& model : model_service->GetModels()) {
    if (model->options && model->options->is_custom_model_options() &&
        model->options->get_custom_model_options()->endpoint.spec() ==
            ai_chat::mojom::kLmStudioEndpoint) {
      count++;
    }
  }
  return count;
}

}  // namespace

class LmStudioModelFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    ModelService::RegisterProfilePrefs(pref_service_.registry());

    model_service_ =
        std::make_unique<ModelService>(&pref_service_, os_crypt_async_.get());
    mock_delegate_ = std::make_unique<MockDelegate>();
    lm_studio_model_fetcher_ = std::make_unique<LmStudioModelFetcher>(
        *model_service_, &pref_service_, mock_delegate_.get());
  }

  ModelService* model_service() { return model_service_.get(); }
  LmStudioModelFetcher* lm_studio_model_fetcher() {
    return lm_studio_model_fetcher_.get();
  }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }
  MockDelegate* mock_delegate() { return mock_delegate_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);
  std::unique_ptr<ModelService> model_service_;
  std::unique_ptr<MockDelegate> mock_delegate_;
  std::unique_ptr<LmStudioModelFetcher> lm_studio_model_fetcher_;
};

TEST_F(LmStudioModelFetcherTest, FetchModelsAddsNewModels) {
  size_t initial_count = model_service()->GetModels().size();
  std::vector<std::string> mock_models = {"llama-3.2", "qwen2.5-coder"};
  EXPECT_CALL(*mock_delegate(), FetchModels(_))
      .WillOnce(base::test::RunOnceCallback<0>(std::move(mock_models)));

  lm_studio_model_fetcher()->FetchModels();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return model_service()->GetModels().size() == initial_count + 2;
  }));
  EXPECT_EQ(2u, CountLmStudioModels(model_service()));
}

TEST_F(LmStudioModelFetcherTest, FetchModelsRemovesObsoleteModels) {
  std::vector<std::string> mock_models = {"llama-3.2", "qwen2.5-coder"};
  EXPECT_CALL(*mock_delegate(), FetchModels(_))
      .WillOnce(base::test::RunOnceCallback<0>(std::move(mock_models)));

  lm_studio_model_fetcher()->FetchModels();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return CountLmStudioModels(model_service()) == 2u; }));

  std::vector<std::string> updated_models = {"llama-3.2"};
  EXPECT_CALL(*mock_delegate(), FetchModels(_))
      .WillOnce(base::test::RunOnceCallback<0>(std::move(updated_models)));

  lm_studio_model_fetcher()->FetchModels();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return CountLmStudioModels(model_service()) == 1u; }));
}

TEST_F(LmStudioModelFetcherTest, FetchModelsHandlesEmptyResponse) {
  size_t initial_count = model_service()->GetModels().size();
  bool callback_called = false;
  EXPECT_CALL(*mock_delegate(), FetchModels(_))
      .WillOnce([&callback_called](
                    LmStudioModelFetcher::Delegate::ModelsCallback callback) {
        std::move(callback).Run(std::nullopt);
        callback_called = true;
      });

  lm_studio_model_fetcher()->FetchModels();

  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_called; }));
  EXPECT_EQ(initial_count, model_service()->GetModels().size());
}

TEST_F(LmStudioModelFetcherTest, FetchModelsHandlesInvalidJSON) {
  size_t initial_count = model_service()->GetModels().size();
  bool callback_called = false;
  EXPECT_CALL(*mock_delegate(), FetchModels(_))
      .WillOnce([&callback_called](
                    LmStudioModelFetcher::Delegate::ModelsCallback callback) {
        std::move(callback).Run(std::nullopt);
        callback_called = true;
      });

  lm_studio_model_fetcher()->FetchModels();

  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_called; }));
  EXPECT_EQ(initial_count, model_service()->GetModels().size());
}

TEST_F(LmStudioModelFetcherTest, PrefChangeTriggersModelFetch) {
  std::vector<std::string> mock_models = {"llama-3.2", "qwen2.5-coder"};
  EXPECT_CALL(*mock_delegate(), FetchModels(_))
      .WillOnce(base::test::RunOnceCallback<0>(std::move(mock_models)));

  pref_service()->SetBoolean(prefs::kBraveAIChatLmStudioFetchEnabled, true);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return CountLmStudioModels(model_service()) == 2u; }));
}

}  // namespace ai_chat
