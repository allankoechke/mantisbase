#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "../test_fixure.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"

class IntegrationAuthTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Server is already started in main.cpp, just get client
        const auto &fixture = TestFixture::instance(mb::json{});
        client = std::make_unique<httplib::Client>(fixture.client());

        // Create admin token for setup
        adminToken = TestHelpers::createTestAdminToken(*client);

        // Create test user entity
        createUserEntity();
    }

    void TearDown() override {
        // Clean up test entities
        TestHelpers::cleanupTestEntity(*client, "test_users", adminToken);
    }

    void createUserEntity() {
        if (adminToken.empty()) {
            throw mb::MantisException(500, "Empty token");
            return;
        }

        nlohmann::json access_rules = {
            {"list", {{"mode", "auth"}}},
            {"get", {{"mode", "auth"}}},
            {"add", {{"mode", "public"}}},
            {"update", {{"mode", "auth"}}},
            {"delete", {{"mode", ""}}}
        };

        nlohmann::json fields = nlohmann::json::array({
            {{"name", "name"}, {"type", "string"}, {"required", true}},
            {{"name", "email"}, {"type", "string"}, {"required", true}, {"unique", true}},
            {{"name", "password"}, {"type", "string"}, {"required", true}}
        });

        httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
        nlohmann::json schema = {
            {"name", "test_users"},
            {"type", "auth"},
            {"fields", fields}
        };

        // Add access rules
        schema["list"] = access_rules["list"];
        schema["get"] = access_rules["get"];
        schema["add"] = access_rules["add"];
        schema["update"] = access_rules["update"];
        schema["delete"] = access_rules["delete"];

        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");

        // Create a test user
        nlohmann::json user = {
            {"name", "Test User"},
            {"email", "testuser@example.com"},
            {"password", TestConfig::getTestPassword()}
        };

        client->Post("/api/v1/entities/test_users", headers,
                     user.dump(), "application/json");
    }

    std::unique_ptr<httplib::Client> client;
    std::string adminToken;
};

TEST_F(IntegrationAuthTest, LoginWithValidCredentials) {
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto res = client->Post("/api/v1/auth/login",
                            login.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].contains("token"));
    EXPECT_FALSE(response["data"]["token"].get<std::string>().empty());
    EXPECT_TRUE(response["data"].contains("user"));
    EXPECT_EQ(response["data"]["user"]["email"], "testuser@example.com");
}

TEST_F(IntegrationAuthTest, LoginWithInvalidPassword) {
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", "wrongpassword"}
    };

    auto res = client->Post("/api/v1/auth/login",
                            login.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 404);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("error"));
}

TEST_F(IntegrationAuthTest, LoginWithInvalidEntity) {
    nlohmann::json login = {
        {"entity", "nonexistent"},
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto res = client->Post("/api/v1/auth/login",
                            login.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 404);
}

TEST_F(IntegrationAuthTest, LoginWithNonAuthEntity) {
    if (adminToken.empty()) {
        GTEST_SKIP() << "Admin token not available";
    }

    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    // Create a non-auth entity
    nlohmann::json schema = {
        {"name", "test_products"},
        {"type", "base"},
        {
            "fields", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}}
            })
        }
    };

    client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");

    nlohmann::json login = {
        {"entity", "test_products"},
        {"identity", "test"},
        {"password", "test"}
    };

    auto res = client->Post("/api/v1/auth/login",
                            login.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);

    // Cleanup
    client->Delete("/api/v1/schemas/test_products", headers);
}

TEST_F(IntegrationAuthTest, RefreshToken) {
    // First login
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto loginRes = client->Post("/api/v1/auth/login",
                                 login.dump(), "application/json");

    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_TRUE(loginRes->status == 200);
    ASSERT_FALSE(loginRes->body.empty());

    // TODO re-enable this after impl refresh token endpoint logic
    // auto loginResponse = nlohmann::json::parse(loginRes->body);
    // EXPECT_TRUE(loginResponse.contains("data") && loginResponse["data"].contains("token"));
    // std::string token = loginResponse["data"]["token"];
    //
    // // Refresh token
    // httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    // auto refreshRes = client->Post("/api/v1/auth/refresh", headers);
    //
    // ASSERT_TRUE(refreshRes != nullptr);
    // EXPECT_EQ(refreshRes->status, 200);
    //
    // auto refreshResponse = nlohmann::json::parse(refreshRes->body);
    // EXPECT_TRUE(refreshResponse.contains("data") && refreshResponse["data"].contains("token"));
    // EXPECT_NE(refreshResponse["data"]["token"].get<std::string>(), token); // Should be different
}

TEST_F(IntegrationAuthTest, RefreshTokenInvalid) {
    httplib::Headers headers = {{"Authorization", "Bearer invalid_token"}};
    auto res = client->Post("/api/v1/auth/refresh", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200); // TODO add proper refresh ...
}

TEST_F(IntegrationAuthTest, Logout) {
    // First login
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto loginRes = client->Post("/api/v1/auth/login",
                                 login.dump(), "application/json");

    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_TRUE(loginRes->status == 200);
    ASSERT_FALSE(loginRes->body.empty());

    auto loginResponse = nlohmann::json::parse(loginRes->body);
    EXPECT_TRUE(loginResponse.contains("data") && loginResponse["data"].contains("token"));
    std::string token = loginResponse["data"]["token"];

    // Logout
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto logoutRes = client->Post("/api/v1/auth/logout", headers);

    ASSERT_TRUE(logoutRes != nullptr);
    EXPECT_EQ(logoutRes->status, 200);
}

TEST_F(IntegrationAuthTest, UseTokenForAuthenticatedRequest) {
    // Login
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto loginRes = client->Post("/api/v1/auth/login",
                                 login.dump(), "application/json");

    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_TRUE(loginRes->status == 200);
    ASSERT_FALSE(loginRes->body.empty());

    auto loginResponse = nlohmann::json::parse(loginRes->body);
    EXPECT_TRUE(loginResponse.contains("data") && loginResponse["data"].contains("token"));
    std::string token = loginResponse["data"]["token"];

    // Use token for authenticated request
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto res = client->Get("/api/v1/entities/test_users", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}
