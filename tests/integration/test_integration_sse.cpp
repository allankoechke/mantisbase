#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoopThread.h>
#include "../common/test_fixture.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

class IntegrationSSETest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        port = getPort();
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        createTestEntity();
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "sse_test_items", adminToken);
        MbServerFixture::TearDown();
    }

    void createTestEntity() {
        if (adminToken.empty()) return;

        const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        const nlohmann::json schema = {
            {"name", "sse_test_items"},
            {"type", "base"},
            {"rules", TestHelpers::publicAccessRules()},
            {
                "fields", nlohmann::json::array({
                    {{"name", "title"}, {"type", "string"}, {"required", true}}
                })
            }
        };

        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
    int port{0};
};

TEST_F(IntegrationSSETest, SSEEndpointReturnsStreamResponse) {
    // Verify the SSE endpoint responds (not 503 anymore)
    auto res = client->Get("/api/v1/realtime?topics=sse_test_items");
    // The SSE endpoint now returns a streaming response, so this test verifies
    // it doesn't return 503. The actual status depends on whether the client
    // can receive the stream - with our synchronous test client, we may get
    // a timeout or the initial response.
    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 503);
}

TEST_F(IntegrationSSETest, SSEEndpointRejects400OnInvalidTopic) {
    auto res = client->Get("/api/v1/realtime?topics=nonexistent_entity");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(IntegrationSSETest, SSEPostUpdateReturns404ForUnknownSession) {
    nlohmann::json body = {
        {"client_id", "nonexistent_session_id"},
        {"topics", nlohmann::json::array({"sse_test_items"})}
    };

    auto res = client->Post("/api/v1/realtime",
                            TestHttp::Headers{{"Authorization", "Bearer " + adminToken}},
                            body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}
