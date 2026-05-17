/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/ai_chat/core/browser/lm_studio/lm_studio_service.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "brave/components/ai_chat/core/common/mojom/lm_studio.mojom.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_chat {

namespace {

constexpr char kLmStudioSuccessResponse[] = "LM Studio";
constexpr char kLmStudioModelsResponse[] = R"({
  "object": "list",
  "data": [
    {
      "id": "llama-3.2-3b-instruct",
      "object": "model"
    },
    {
      "id": "qwen2.5-coder-7b-instruct",
      "object": "model"
    }
  ]
})";

}  // namespace

class LmStudioServiceTest : public testing::Test {
 public:
  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    lm_studio_client_ = std::make_unique<LmStudioService>(
        shared_url_loader_factory_, /*model_fetcher=*/nullptr);
  }

  LmStudioService* lm_studio_client() { return lm_studio_client_.get(); }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<LmStudioService> lm_studio_client_;
};

TEST_F(LmStudioServiceTest, ConnectedSuccess) {
  test_url_loader_factory()->AddResponse(ai_chat::mojom::kLmStudioBaseUrl,
                                         kLmStudioSuccessResponse);

  base::test::TestFuture<bool> future;
  lm_studio_client()->IsConnected(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(LmStudioServiceTest, ConnectedFailure) {
  test_url_loader_factory()->AddResponse(ai_chat::mojom::kLmStudioBaseUrl, "",
                                         net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<bool> future;
  lm_studio_client()->IsConnected(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(LmStudioServiceTest, ConnectedNoResponse) {
  test_url_loader_factory()->AddResponse(
      GURL(ai_chat::mojom::kLmStudioBaseUrl),
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  base::test::TestFuture<bool> future;
  lm_studio_client()->IsConnected(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(LmStudioServiceTest, FetchModelsSuccess) {
  test_url_loader_factory()->AddResponse(
      ai_chat::mojom::kLmStudioListModelsAPIEndpoint,
      kLmStudioModelsResponse);

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  lm_studio_client()->FetchModels(future.GetCallback());

  const auto& models = future.Get();
  ASSERT_TRUE(models.has_value());
  ASSERT_EQ(2u, models->size());
  EXPECT_EQ("llama-3.2-3b-instruct", (*models)[0]);
  EXPECT_EQ("qwen2.5-coder-7b-instruct", (*models)[1]);
}

TEST_F(LmStudioServiceTest, FetchModelsNoResponse) {
  test_url_loader_factory()->AddResponse(
      GURL(ai_chat::mojom::kLmStudioListModelsAPIEndpoint),
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  lm_studio_client()->FetchModels(future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(LmStudioServiceTest, FetchModelsInvalidJSON) {
  test_url_loader_factory()->AddResponse(
      ai_chat::mojom::kLmStudioListModelsAPIEndpoint, "{invalid json}");

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  lm_studio_client()->FetchModels(future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(LmStudioServiceTest, FetchModelsMissingDataKey) {
  test_url_loader_factory()->AddResponse(
      ai_chat::mojom::kLmStudioListModelsAPIEndpoint, R"({"models": []})");

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  lm_studio_client()->FetchModels(future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(LmStudioServiceTest, ParseModelsResponse_Valid) {
  auto result = lm_studio_client()->ParseModelsResponse(kLmStudioModelsResponse);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(2u, result->size());
  EXPECT_EQ("llama-3.2-3b-instruct", (*result)[0]);
  EXPECT_EQ("qwen2.5-coder-7b-instruct", (*result)[1]);
}

TEST_F(LmStudioServiceTest, ParseModelsResponse_InvalidJSON) {
  auto result = lm_studio_client()->ParseModelsResponse("{invalid json}");
  EXPECT_FALSE(result.has_value());
}

TEST_F(LmStudioServiceTest, ParseModelsResponse_MissingDataKey) {
  auto result = lm_studio_client()->ParseModelsResponse(R"({"models": []})");
  EXPECT_FALSE(result.has_value());
}

TEST_F(LmStudioServiceTest, ParseModelsResponse_EmptyData) {
  auto result = lm_studio_client()->ParseModelsResponse(R"({"data": []})");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(0u, result->size());
}

TEST_F(LmStudioServiceTest, ParseModelsResponse_InvalidModelStructure) {
  constexpr char kInvalidStructure[] = R"({
    "data": [
      {"object": "model"},
      {"id": "valid-model"}
    ]
  })";

  auto result = lm_studio_client()->ParseModelsResponse(kInvalidStructure);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(1u, result->size());
  EXPECT_EQ("valid-model", (*result)[0]);
}

}  // namespace ai_chat
