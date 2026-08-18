#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "../common/test_fixture.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

namespace {

nlohmann::json parseBody(const TestHttp::Response &res) {
    return nlohmann::json::parse(res.body);
}

class IntegrationMiddlewareTest : public MbServerFixture {
protected:
    static constexpr const char *kEntity = "test_users";
    static constexpr const char *kEmail = "middleware_test@example.com";

    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        ASSERT_FALSE(adminToken.empty());
        createUserEntity();
        userToken = TestHelpers::loginUser(*client, kEntity, kEmail, TestConfig::getTestPassword());
        ASSERT_FALSE(userToken.empty());
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, kEntity, adminToken);
        MbServerFixture::TearDown();
    }

    void createUserEntity() {
        TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
        nlohmann::json schema = {
            {"name", kEntity},
            {"type", "auth"},
            {"list", {{"mode", "auth"}}},
            {"get", {{"mode", "auth"}}},
            {"add", {{"mode", "public"}}},
            {"update", {{"mode", "auth"}}},
            {"delete", {{"mode", ""}}},
            {
                "fields", nlohmann::json::array({
                    {{"name", "name"}, {"type", "string"}, {"required", true}},
                    {{"name", "email"}, {"type", "string"}, {"required", true}, {"unique", true}},
                    {{"name", "password"}, {"type", "string"}, {"required", true}}
                })
            }
        };

        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");

        nlohmann::json user = {
            {"name", "Middleware Test User"},
            {"email", kEmail},
            {"password", TestConfig::getTestPassword()}
        };
        client->Post("/api/v1/entities/" + std::string(kEntity), headers, user.dump(), "application/json");
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
    std::string userToken;
};

} // namespace

TEST_F(IntegrationMiddlewareTest, RequireExprEvalDeniesGuest) {
    auto res = client->Get("/api/v1/test/middleware/expr");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
    EXPECT_EQ(parseBody(*res)["error"].get<std::string>(), "Access denied!");
}

TEST_F(IntegrationMiddlewareTest, RequireExprEvalAllowsMatchingUser) {
    auto res = client->Get("/api/v1/test/middleware/expr",
                            {{"Authorization", "Bearer " + userToken}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_TRUE(parseBody(*res)["data"]["ok"].get<bool>());
}

TEST_F(IntegrationMiddlewareTest, RequireExprEvalDeniesAdminWhenExprRequiresEntity) {
    auto res = client->Get("/api/v1/test/middleware/expr",
                            {{"Authorization", "Bearer " + adminToken}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
    EXPECT_EQ(parseBody(*res)["error"].get<std::string>(), "Access denied!");
}

TEST_F(IntegrationMiddlewareTest, RequireEntityAuthRequiresToken) {
    auto res = client->Get("/api/v1/test/middleware/entity-auth");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationMiddlewareTest, RequireEntityAuthAllowsMatchingEntity) {
    auto res = client->Get("/api/v1/test/middleware/entity-auth",
                            {{"Authorization", "Bearer " + userToken}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}

TEST_F(IntegrationMiddlewareTest, RequireEntityAuthDeniesAdmin) {
    auto res = client->Get("/api/v1/test/middleware/entity-auth",
                            {{"Authorization", "Bearer " + adminToken}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
    EXPECT_EQ(parseBody(*res)["error"].get<std::string>(), "Auth required from entity `test_users`.");
}

TEST_F(IntegrationMiddlewareTest, RequireAdminOrEntityAuthRequiresToken) {
    auto res = client->Get("/api/v1/test/middleware/admin-or-entity");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationMiddlewareTest, RequireAdminOrEntityAuthAllowsUser) {
    auto res = client->Get("/api/v1/test/middleware/admin-or-entity",
                            {{"Authorization", "Bearer " + userToken}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}

TEST_F(IntegrationMiddlewareTest, RequireAdminOrEntityAuthAllowsAdmin) {
    auto res = client->Get("/api/v1/test/middleware/admin-or-entity",
                            {{"Authorization", "Bearer " + adminToken}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}
