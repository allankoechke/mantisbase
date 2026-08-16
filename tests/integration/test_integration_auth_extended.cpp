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

class IntegrationAuthExtendedTest : public MbServerFixture {
protected:
    static constexpr const char *kEntity = "test_users";
    static constexpr const char *kEmail = "testuser@example.com";

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
        TestHelpers::cleanupTestEntity(*client, "test_denied_list", adminToken);
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
            {"name", "Test User"},
            {"email", kEmail},
            {"password", TestConfig::getTestPassword()}
        };
        client->Post("/api/v1/entities/" + std::string(kEntity), headers, user.dump(), "application/json");
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
    std::string userToken;
};

class IntegrationAdminEditsGateTest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        ASSERT_FALSE(adminToken.empty());
        TestHelpers::setEnvVar("MB_DISABLE_ADMIN_MUTATIONS");
    }

    void TearDown() override {
        TestHelpers::setEnvVar("MB_DISABLE_ADMIN_MUTATIONS");
        MbServerFixture::TearDown();
    }

    std::string firstAdminId() const {
        TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
        auto res = client->Get("/api/v1/sys/admins", headers);
        EXPECT_TRUE(res != nullptr);
        EXPECT_EQ(res->status, 200);
        auto body = parseBody(*res);
        EXPECT_TRUE(body.contains("data"));
        EXPECT_TRUE(body["data"].contains("items"));
        EXPECT_FALSE(body["data"]["items"].empty());
        return body["data"]["items"][0]["id"].get<std::string>();
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
};

} // namespace

// --- Auth verify ---

TEST_F(IntegrationAuthExtendedTest, VerifyWithoutToken) {
    auto res = client->Get("/api/v1/auth/verify");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Missing or invalid auth token");
}

TEST_F(IntegrationAuthExtendedTest, VerifyWithInvalidToken) {
    auto res = client->Get("/api/v1/auth/verify",
                            {{"Authorization", "Bearer invalid.token.here"}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationAuthExtendedTest, VerifyWithValidJwt) {
    auto res = client->Get("/api/v1/auth/verify",
                            {{"Authorization", "Bearer " + userToken}});
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    auto body = parseBody(*res);
    EXPECT_EQ(body["data"]["status"].get<std::string>(), "OK");
}

TEST_F(IntegrationAuthExtendedTest, VerifyWithApiKey) {
    TestHttp::Headers jwtHeaders = {{"Authorization", "Bearer " + userToken}};
    auto createRes = client->Post("/api/v1/auth/test_users/api-keys", jwtHeaders,
                                    R"({"label":"verify test key"})", "application/json");
    ASSERT_TRUE(createRes != nullptr);
    ASSERT_EQ(createRes->status, 201);
    auto created = parseBody(*createRes);
    const auto apiKey = created["data"]["key"].get<std::string>();
    ASSERT_TRUE(apiKey.starts_with("mb_sk_"));

    auto verifyRes = client->Get("/api/v1/auth/verify",
                                 {{"Authorization", "Bearer " + apiKey}});
    ASSERT_TRUE(verifyRes != nullptr);
    EXPECT_EQ(verifyRes->status, 200);

    const auto keyId = created["data"]["id"].get<std::string>();
    client->Delete("/api/v1/auth/test_users/api-keys/" + keyId, jwtHeaders);
}

TEST_F(IntegrationAuthExtendedTest, VerifyAfterLogout) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};

    auto verifyBefore = client->Get("/api/v1/auth/verify", headers);
    ASSERT_TRUE(verifyBefore != nullptr);
    EXPECT_EQ(verifyBefore->status, 200);

    auto logoutRes = client->Post("/api/v1/auth/test_users/logout", headers, "", "application/json");
    ASSERT_TRUE(logoutRes != nullptr);
    EXPECT_EQ(logoutRes->status, 200);

    auto verifyAfter = client->Get("/api/v1/auth/verify", headers);
    ASSERT_TRUE(verifyAfter != nullptr);
    EXPECT_EQ(verifyAfter->status, 401);
    auto body = parseBody(*verifyAfter);
    EXPECT_EQ(body["error"].get<std::string>(), "Session expired or revoked");
}

// --- API key REST ---

TEST_F(IntegrationAuthExtendedTest, CreateApiKeyRequiresAuth) {
    auto res = client->Post("/api/v1/auth/test_users/api-keys",
                            R"({"label":"unauth"})", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationAuthExtendedTest, CreateApiKeyWithUserJwt) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};
    auto res = client->Post("/api/v1/auth/test_users/api-keys", headers,
                            R"({"label":"integration key"})", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);
    auto body = parseBody(*res);
    EXPECT_TRUE(body["data"]["key"].get<std::string>().starts_with("mb_sk_"));
}

TEST_F(IntegrationAuthExtendedTest, CreateListRevokeApiKey) {
    TestHttp::Headers jwtHeaders = {{"Authorization", "Bearer " + userToken}};

    auto createRes = client->Post("/api/v1/auth/test_users/api-keys", jwtHeaders,
                                  R"({"label":"lifecycle key"})", "application/json");
    ASSERT_TRUE(createRes != nullptr);
    ASSERT_EQ(createRes->status, 201);
    auto created = parseBody(*createRes);
    const auto apiKey = created["data"]["key"].get<std::string>();
    const auto keyId = created["data"]["id"].get<std::string>();

    auto listRes = client->Get("/api/v1/auth/test_users/api-keys", jwtHeaders);
    ASSERT_TRUE(listRes != nullptr);
    EXPECT_EQ(listRes->status, 200);
    auto listed = parseBody(*listRes);
    ASSERT_TRUE(listed["data"].is_array());
    bool found = false;
    for (const auto &item : listed["data"]) {
        if (item["id"].get<std::string>() == keyId) {
            found = true;
            EXPECT_FALSE(item.contains("key"));
        }
    }
    EXPECT_TRUE(found);

    TestHttp::Headers keyHeaders = {{"Authorization", "Bearer " + apiKey}};
    auto entityRes = client->Get("/api/v1/entities/test_users", keyHeaders);
    ASSERT_TRUE(entityRes != nullptr);
    EXPECT_EQ(entityRes->status, 200);

    auto revokeRes = client->Delete("/api/v1/auth/test_users/api-keys/" + keyId, jwtHeaders);
    ASSERT_TRUE(revokeRes != nullptr);
    EXPECT_EQ(revokeRes->status, 200);

    auto afterRevoke = client->Get("/api/v1/entities/test_users", keyHeaders);
    ASSERT_TRUE(afterRevoke != nullptr);
    EXPECT_EQ(afterRevoke->status, 401);
}

TEST_F(IntegrationAuthExtendedTest, SysApiKeyAdminOnly) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};
    auto res = client->Post("/api/v1/sys/api-keys", headers,
                            R"({"label":"should fail"})", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Admin auth required to access this resource.");
}

TEST_F(IntegrationAuthExtendedTest, SysApiKeyWithAdminToken) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    auto res = client->Post("/api/v1/sys/api-keys", headers,
                            R"({"label":"admin integration key"})", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);
    auto body = parseBody(*res);
    EXPECT_TRUE(body["data"]["key"].get<std::string>().starts_with("mb_sk_"));
}

// --- 403 permission tests ---

TEST_F(IntegrationAuthExtendedTest, UserTokenOnAdminRouteReturns403) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};
    nlohmann::json schema = {
        {"name", "blocked_schema"},
        {"type", "base"},
        {"fields", nlohmann::json::array()}
    };
    auto res = client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Admin auth required to access this resource.");
}

TEST_F(IntegrationAuthExtendedTest, CustomRuleDeniesAccess) {
    TestHttp::Headers adminHeaders = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json schema = {
        {"name", "test_denied_list"},
        {"type", "base"},
        {
            "rules", {
                {"list", {{"mode", "custom"}, {"expr", "auth.entity == \"mb_admins\""}}},
                {"get", {{"mode", "public"}}},
                {"add", {{"mode", "public"}}},
                {"update", {{"mode", "public"}}},
                {"delete", {{"mode", "public"}}}
            }
        },
        {
            "fields", nlohmann::json::array({
                {{"name", "title"}, {"type", "string"}}
            })
        }
    };
    auto createSchema = client->Post("/api/v1/schemas", adminHeaders, schema.dump(), "application/json");
    ASSERT_TRUE(createSchema != nullptr);
    ASSERT_EQ(createSchema->status, 201);

    TestHttp::Headers userHeaders = {{"Authorization", "Bearer " + userToken}};
    auto res = client->Get("/api/v1/entities/test_denied_list", userHeaders);
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Access denied!");
}

TEST_F(IntegrationAuthExtendedTest, AdminRouteWithValidUserNotAdmin) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};
    auto res = client->Get("/api/v1/sys/logs", headers);
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Admin auth required to access this resource.");
}

// --- Logout and refresh ---

TEST_F(IntegrationAuthExtendedTest, LogoutWithoutToken) {
    auto res = client->Post("/api/v1/auth/test_users/logout", "", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Valid token required to logout");
}

TEST_F(IntegrationAuthExtendedTest, LogoutWithInvalidToken) {
    auto res = client->Post("/api/v1/auth/test_users/logout",
                            {{"Authorization", "Bearer invalid.token.here"}},
                            "", "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationAuthExtendedTest, LogoutThenRefreshFails) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};
    ASSERT_EQ(client->Post("/api/v1/auth/test_users/logout", headers, "", "application/json")->status, 200);

    auto refreshRes = client->Post("/api/v1/auth/test_users/refresh", headers, "", "application/json");
    ASSERT_TRUE(refreshRes != nullptr);
    EXPECT_EQ(refreshRes->status, 401);
}

TEST_F(IntegrationAuthExtendedTest, LogoutThenEntityAccessFails) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};
    ASSERT_EQ(client->Post("/api/v1/auth/test_users/logout", headers, "", "application/json")->status, 200);

    auto listRes = client->Get("/api/v1/entities/test_users", headers);
    ASSERT_TRUE(listRes != nullptr);
    EXPECT_EQ(listRes->status, 401);
}

TEST_F(IntegrationAuthExtendedTest, RefreshTokenSuccess) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};
    auto refreshRes = client->Post("/api/v1/auth/test_users/refresh", headers, "", "application/json");
    ASSERT_TRUE(refreshRes != nullptr);
    EXPECT_EQ(refreshRes->status, 200);
    auto body = parseBody(*refreshRes);
    EXPECT_TRUE(body["data"].contains("token"));
    EXPECT_FALSE(body["data"]["token"].get<std::string>().empty());
}

// --- MB_DISABLE_ADMIN_MUTATIONS / envGateMiddleware ---

TEST_F(IntegrationAdminEditsGateTest, AdminPatchAllowedWhenGateOpen) {
    const auto adminId = firstAdminId();
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json patch = {{"email", TestConfig::getAdminEmail()}};
    auto res = client->Patch("/api/v1/sys/admins/" + adminId, headers, patch.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}

TEST_F(IntegrationAdminEditsGateTest, AdminPatchBlockedWhenGateClosed) {
    TestHelpers::setEnvVar("MB_DISABLE_ADMIN_MUTATIONS", "true");
    const auto adminId = firstAdminId();
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json patch = {{"email", TestConfig::getAdminEmail()}};
    auto res = client->Patch("/api/v1/sys/admins/" + adminId, headers, patch.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 503);
    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Resource action temporarily disabled");
}

TEST_F(IntegrationAdminEditsGateTest, AdminCreateAllowedWhenGateOpen) {
    const std::string email = "admin_gate_open_" + mb::generateShortId(8) + "@test.com";
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json body = {
        {"email", email},
        {"password", TestConfig::getTestPassword()}
    };
    auto res = client->Post("/api/v1/sys/admins", headers, body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);
}

TEST_F(IntegrationAdminEditsGateTest, AdminCreateBlockedWhenGateClosed) {
    TestHelpers::setEnvVar("MB_DISABLE_ADMIN_MUTATIONS", "true");
    const std::string email = "admin_gate_closed_" + mb::generateShortId(8) + "@test.com";
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json body = {
        {"email", email},
        {"password", TestConfig::getTestPassword()}
    };
    auto res = client->Post("/api/v1/sys/admins", headers, body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 503);
    auto parsed = parseBody(*res);
    EXPECT_EQ(parsed["error"].get<std::string>(), "Resource action temporarily disabled");
}

TEST_F(IntegrationAdminEditsGateTest, AdminEditsIgnoredWhenEnvNotTruthy) {
    TestHelpers::setEnvVar("MB_DISABLE_ADMIN_MUTATIONS", "false");
    const std::string email = "admin_gate_ignored_" + mb::generateShortId(8) + "@test.com";
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json body = {
        {"email", email},
        {"password", TestConfig::getTestPassword()}
    };
    auto res = client->Post("/api/v1/sys/admins", headers, body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);
}
