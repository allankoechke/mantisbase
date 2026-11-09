#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <regex>
#include "../common/test_fixture.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

class IntegrationAuthTest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        createUserEntity();
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "test_users", adminToken);
        MbServerFixture::TearDown();
    }

    void createUserEntity() {
        if (adminToken.empty()) {
            throw mb::MbException(500, "Empty token");
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

        TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
        nlohmann::json schema = {
            {"name", "test_users"},
            {"type", "auth"},
            {"rules", access_rules},
            {"fields", fields}
        };

        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");

        nlohmann::json user = {
            {"name", "Test User"},
            {"email", "testuser@example.com"},
            {"password", TestConfig::getTestPassword()}
        };

        client->Post("/api/v1/entities/test_users", headers,
                     user.dump(), "application/json");
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
};

TEST_F(IntegrationAuthTest, LoginWithValidCredentials) {
    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto res = client->Post("/api/v1/auth/test_users/login",
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
        {"identity", "testuser@example.com"},
        {"password", "wrongpassword"}
    };

    auto res = client->Post("/api/v1/auth/test_users/login",
                            login.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 404);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("error"));
}

TEST_F(IntegrationAuthTest, LoginWithInvalidEntity) {
    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto res = client->Post("/api/v1/auth/nonexistent/login",
                            login.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 404);
}

TEST_F(IntegrationAuthTest, LoginWithNonAuthEntity) {
    if (adminToken.empty()) {
        GTEST_SKIP() << "Admin token not available";
    }

    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

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
        {"identity", "test"},
        {"password", "test"}
    };

    auto res = client->Post("/api/v1/auth/test_products/login",
                            login.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 404);

    client->Delete("/api/v1/schemas/test_products", headers);
}

TEST_F(IntegrationAuthTest, CookieAuthAcceptedWithoutBearer) {
    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto loginRes = client->Post("/api/v1/auth/test_users/login",
                                 login.dump(), "application/json");
    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_EQ(loginRes->status, 200);

    const auto loginResponse = nlohmann::json::parse(loginRes->body);
    const std::string token = loginResponse["data"]["token"].get<std::string>();

    TestHttp::Headers cookieHeader = {{"Cookie", std::string("mb_token=") + token}};
    auto verifyRes = client->Get("/api/v1/auth/verify", cookieHeader);
    ASSERT_TRUE(verifyRes != nullptr);
    EXPECT_EQ(verifyRes->status, 200);

    const auto verifyBody = nlohmann::json::parse(verifyRes->body);
    EXPECT_EQ(verifyBody["data"]["status"].get<std::string>(), "OK");
}

TEST_F(IntegrationAuthTest, RefreshToken) {
    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto loginRes = client->Post("/api/v1/auth/test_users/login",
                                 login.dump(), "application/json");

    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_TRUE(loginRes->status == 200);
    ASSERT_FALSE(loginRes->body.empty());
}

TEST_F(IntegrationAuthTest, RefreshTokenInvalid) {
    TestHttp::Headers headers = {{"Authorization", "Bearer invalid_token"}};
    auto res = client->Post("/api/v1/auth/test_users/refresh", headers, "", "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationAuthTest, Logout) {
    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto loginRes = client->Post("/api/v1/auth/test_users/login",
                                 login.dump(), "application/json");

    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_TRUE(loginRes->status == 200);
    ASSERT_FALSE(loginRes->body.empty());

    auto loginResponse = nlohmann::json::parse(loginRes->body);
    EXPECT_TRUE(loginResponse.contains("data") && loginResponse["data"].contains("token"));
    std::string token = loginResponse["data"]["token"];

    TestHttp::Headers headers = {{"Authorization", "Bearer " + token}};
    auto logoutRes = client->Post("/api/v1/auth/test_users/logout", headers, "", "application/json");

    ASSERT_TRUE(logoutRes != nullptr);
    EXPECT_EQ(logoutRes->status, 200);
}

TEST_F(IntegrationAuthTest, UseTokenForAuthenticatedRequest) {
    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };

    auto loginRes = client->Post("/api/v1/auth/test_users/login",
                                 login.dump(), "application/json");

    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_TRUE(loginRes->status == 200);
    ASSERT_FALSE(loginRes->body.empty());

    auto loginResponse = nlohmann::json::parse(loginRes->body);
    EXPECT_TRUE(loginResponse.contains("data") && loginResponse["data"].contains("token"));
    std::string token = loginResponse["data"]["token"];

    TestHttp::Headers headers = {{"Authorization", "Bearer " + token}};
    auto res = client->Get("/api/v1/entities/test_users", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}

TEST_F(IntegrationAuthTest, AuthEntityFilterAllowsMatchingEntity) {
    if (adminToken.empty()) {
        GTEST_SKIP() << "Admin token not available";
    }

    TestHttp::Headers adminHeaders = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json schema = {
        {"name", "filtered_posts"},
        {"type", "base"},
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}, {"required", true}}
        })},
        {"rules", {
            {"list", {{"mode", "auth"}, {"entity", "test_users"}}},
            {"get", {{"mode", "auth"}, {"entity", "test_users"}}},
            {"add", {{"mode", "public"}}},
            {"update", {{"mode", "auth"}, {"entity", "test_users"}}},
            {"delete", {{"mode", ""}}}
        }}
    };

    auto createRes = client->Post("/api/v1/schemas", adminHeaders, schema.dump(), "application/json");
    ASSERT_TRUE(createRes != nullptr);
    ASSERT_EQ(createRes->status, 201);

    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };
    auto loginRes = client->Post("/api/v1/auth/test_users/login", login.dump(), "application/json");
    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_EQ(loginRes->status, 200);
    const auto token = nlohmann::json::parse(loginRes->body)["data"]["token"].get<std::string>();
    TestHttp::Headers userHeaders = {{"Authorization", "Bearer " + token}};

    auto listRes = client->Get("/api/v1/entities/filtered_posts", userHeaders);
    ASSERT_TRUE(listRes != nullptr);
    EXPECT_EQ(listRes->status, 200);

    auto adminListRes = client->Get("/api/v1/entities/filtered_posts", adminHeaders);
    ASSERT_TRUE(adminListRes != nullptr);
    EXPECT_EQ(adminListRes->status, 200);

    client->Delete("/api/v1/schemas/filtered_posts", adminHeaders);
}

TEST_F(IntegrationAuthTest, AuthEntityFilterDeniesNonMatchingEntity) {
    if (adminToken.empty()) {
        GTEST_SKIP() << "Admin token not available";
    }

    TestHttp::Headers adminHeaders = {{"Authorization", "Bearer " + adminToken}};

    nlohmann::json editorsSchema = {
        {"name", "editors_only"},
        {"type", "auth"},
        {"fields", nlohmann::json::array({
            {{"name", "email"}, {"type", "string"}, {"required", true}, {"unique", true}},
            {{"name", "password"}, {"type", "string"}, {"required", true}}
        })},
        {"rules", {
            {"list", {{"mode", "public"}}},
            {"add", {{"mode", "public"}}}
        }}
    };
    client->Post("/api/v1/schemas", adminHeaders, editorsSchema.dump(), "application/json");
    client->Post("/api/v1/entities/editors_only", adminHeaders,
                 nlohmann::json{{"email", "editor@example.com"}, {"password", TestConfig::getTestPassword()}}.dump(),
                 "application/json");

    nlohmann::json schema = {
        {"name", "editor_posts"},
        {"type", "base"},
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}, {"required", true}}
        })},
        {"rules", {
            {"list", {{"mode", "auth"}, {"entity", "editors_only"}}}
        }}
    };
    auto createRes = client->Post("/api/v1/schemas", adminHeaders, schema.dump(), "application/json");
    ASSERT_TRUE(createRes != nullptr);
    ASSERT_EQ(createRes->status, 201);

    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };
    auto loginRes = client->Post("/api/v1/auth/test_users/login", login.dump(), "application/json");
    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_EQ(loginRes->status, 200);
    const auto token = nlohmann::json::parse(loginRes->body)["data"]["token"].get<std::string>();
    TestHttp::Headers userHeaders = {{"Authorization", "Bearer " + token}};

    auto listRes = client->Get("/api/v1/entities/editor_posts", userHeaders);
    ASSERT_TRUE(listRes != nullptr);
    EXPECT_EQ(listRes->status, 403);

    client->Delete("/api/v1/schemas/editor_posts", adminHeaders);
    client->Delete("/api/v1/schemas/editors_only", adminHeaders);
}

namespace {

std::string codeFromEmail(const std::string &body) {
    static const std::regex re(R"(\b(\d{6})\b)");
    std::smatch match;
    if (std::regex_search(body, match, re))
        return match[1].str();
    return {};
}

class MockSmtp {
public:
    explicit MockSmtp(mb::MantisBase &app) : app_(app) {
        app_.mailer().clearSentMessages();
        app_.mailer().clearRateLimits();
    }

    ~MockSmtp() {
        try {
            app_.mailer().clearSentMessages();
            app_.mailer().clearRateLimits();
            app_.settings().setScriptingValue("smtp", {
                                                   {"host", ""},
                                                   {"port", 587},
                                                   {"user", ""},
                                                   {"password", ""},
                                                   {"from", ""},
                                                   {"tls", true}
                                               });
        } catch (...) {
        }
    }

    void enable() {
        app_.settings().setScriptingValue("smtp", {
                                               {"host", "mock"},
                                               {"port", 587},
                                               {"user", "mailer"},
                                               {"password", "s3cret"},
                                               {"from", "app@example.com"},
                                               {"tls", true}
                                           });
        app_.mailer().clearSentMessages();
        app_.mailer().clearRateLimits();
    }

private:
    mb::MantisBase &app_;
};

std::string requestCode(TestHttp::Client &client, mb::MantisBase &app, const std::string &email) {
    app.mailer().clearSentMessages();
    app.mailer().clearRateLimits();
    const nlohmann::json body = {{"email", email}};
    auto res = client.Post("/api/v1/auth/test_users/otp/request", body.dump(), "application/json");
    EXPECT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(app.mailer().sentMessages().size(), 1u);
    if (app.mailer().sentMessages().empty())
        return {};
    const auto code = codeFromEmail(app.mailer().sentMessages().front().body_text);
    EXPECT_EQ(code.size(), 6u);
    EXPECT_EQ(nlohmann::json::parse(res->body).dump().find(code), std::string::npos);
    return code;
}

} // namespace

TEST_F(IntegrationAuthTest, OtpRequestRequiresSmtp) {
    MockSmtp smtp(mantis());
    EXPECT_FALSE(mantis().mailer().isConfigured());

    const nlohmann::json body = {{"email", "testuser@example.com"}};
    auto res = client->Post("/api/v1/auth/test_users/otp/request", body.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);
    const auto response = nlohmann::json::parse(res->body);
    EXPECT_NE(response["error"].get<std::string>().find("not configured"), std::string::npos);
    EXPECT_TRUE(response["data"].empty());
    EXPECT_TRUE(mantis().mailer().sentMessages().empty());

    auto missing = client->Post("/api/v1/auth/test_users/otp/request", "{}", "application/json");
    ASSERT_TRUE(missing != nullptr);
    EXPECT_EQ(missing->status, 400);

    auto notAuth = client->Post("/api/v1/auth/mb_admins/otp/request", body.dump(), "application/json");
    ASSERT_TRUE(notAuth != nullptr);
    EXPECT_EQ(notAuth->status, 404);
}

TEST_F(IntegrationAuthTest, OtpLoginWithEmailedCode) {
    MockSmtp smtp(mantis());
    smtp.enable();
    ASSERT_TRUE(mantis().mailer().isConfigured());

    const nlohmann::json unknown = {{"email", "nobody@example.com"}};
    auto missing = client->Post("/api/v1/auth/test_users/otp/request", unknown.dump(), "application/json");
    ASSERT_TRUE(missing != nullptr);
    EXPECT_EQ(missing->status, 200);
    EXPECT_TRUE(nlohmann::json::parse(missing->body)["data"]["sent"].get<bool>());
    EXPECT_TRUE(mantis().mailer().sentMessages().empty());

    auto invalid = client->Post("/api/v1/auth/test_users/otp/request",
                                nlohmann::json{{"email", "not-an-email"}}.dump(),
                                "application/json");
    ASSERT_TRUE(invalid != nullptr);
    EXPECT_EQ(invalid->status, 400);
    EXPECT_EQ(nlohmann::json::parse(invalid->body)["error"].get<std::string>(), "Invalid email address");

    const auto code = requestCode(*client, mantis(), "TestUser@Example.com");
    ASSERT_EQ(code.size(), 6u);

    nlohmann::json login = {{"email", "testuser@example.com"}, {"otp", " " + code + " "}};
    auto loginRes = client->Post("/api/v1/auth/test_users/otp/login", login.dump(), "application/json");
    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_EQ(loginRes->status, 200);
    const auto loginBody = nlohmann::json::parse(loginRes->body);
    const auto token = loginBody["data"]["token"].get<std::string>();
    EXPECT_FALSE(token.empty());
    EXPECT_EQ(loginBody["data"]["user"]["email"].get<std::string>(), "testuser@example.com");
    EXPECT_FALSE(loginBody["data"]["user"].contains("password"));

    TestHttp::Headers headers = {{"Authorization", "Bearer " + token}};
    auto verifyRes = client->Get("/api/v1/auth/verify", headers);
    ASSERT_TRUE(verifyRes != nullptr);
    EXPECT_EQ(verifyRes->status, 200);

    auto reuse = client->Post("/api/v1/auth/test_users/otp/login",
                              nlohmann::json{{"email", "testuser@example.com"}, {"otp", code}}.dump(),
                              "application/json");
    ASSERT_TRUE(reuse != nullptr);
    EXPECT_EQ(reuse->status, 401);
}

TEST_F(IntegrationAuthTest, OtpLoginRejectsWrongCode) {
    MockSmtp smtp(mantis());
    smtp.enable();

    const auto code = requestCode(*client, mantis(), "testuser@example.com");
    ASSERT_EQ(code.size(), 6u);

    auto malformed = client->Post("/api/v1/auth/test_users/otp/login",
                                  nlohmann::json{{"email", "testuser@example.com"}, {"otp", "12"}}.dump(),
                                  "application/json");
    ASSERT_TRUE(malformed != nullptr);
    EXPECT_EQ(malformed->status, 400);

    for (int i = 0; i < 5; ++i) {
        auto wrong = client->Post("/api/v1/auth/test_users/otp/login",
                                  nlohmann::json{{"email", "testuser@example.com"}, {"otp", "000000"}}.dump(),
                                  "application/json");
        ASSERT_TRUE(wrong != nullptr);
        EXPECT_EQ(wrong->status, 401);
    }

    auto exhausted = client->Post("/api/v1/auth/test_users/otp/login",
                                  nlohmann::json{{"email", "testuser@example.com"}, {"otp", code}}.dump(),
                                  "application/json");
    ASSERT_TRUE(exhausted != nullptr);
    EXPECT_EQ(exhausted->status, 401);
}
