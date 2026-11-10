#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <soci/soci.h>
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

void setAuthMethod(TestHttp::Client &client, const std::string &adminToken,
                   const std::string &method, bool enabled) {
    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    const nlohmann::json patch = {{"auth", {{method, enabled}}}};
    auto res = client.Patch("/api/v1/schemas/test_users", headers, patch.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    ASSERT_EQ(res->status, 200) << res->body;
}

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

TEST_F(IntegrationAuthTest, AuthMethodsDefaultToPasswordOnly) {
    auto methodsRes = client->Get("/api/v1/auth/test_users/methods");
    ASSERT_TRUE(methodsRes != nullptr);
    ASSERT_EQ(methodsRes->status, 200);
    const auto methods = nlohmann::json::parse(methodsRes->body)["data"];
    EXPECT_TRUE(methods["password"].get<bool>());
    EXPECT_FALSE(methods["otp"].get<bool>());
    EXPECT_FALSE(methods["oauth"].get<bool>());
    EXPECT_FALSE(methods["requireEmailVerification"].get<bool>());

    const TestHttp::Headers admin = {{"Authorization", "Bearer " + adminToken}};
    auto schemaRes = client->Get("/api/v1/schemas/test_users", admin);
    ASSERT_TRUE(schemaRes != nullptr);
    ASSERT_EQ(schemaRes->status, 200);
    const auto stored = nlohmann::json::parse(schemaRes->body)["data"]["schema"]["auth"];
    EXPECT_TRUE(stored["password"].get<bool>());
    EXPECT_FALSE(stored["otp"].get<bool>());
    EXPECT_FALSE(stored["oauth"].get<bool>());
    EXPECT_FALSE(stored["requireEmailVerification"].get<bool>());

    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };
    auto loginRes = client->Post("/api/v1/auth/test_users/login", login.dump(), "application/json");
    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_EQ(loginRes->status, 200);
    const auto userToken = nlohmann::json::parse(loginRes->body)["data"]["token"].get<std::string>();

    const nlohmann::json enableOtp = {{"auth", {{"otp", true}}}};
    const TestHttp::Headers user = {{"Authorization", "Bearer " + userToken}};
    auto denied = client->Patch("/api/v1/schemas/test_users", user, enableOtp.dump(), "application/json");
    ASSERT_TRUE(denied != nullptr);
    EXPECT_EQ(denied->status, 403);

    auto anonymous = client->Patch("/api/v1/schemas/test_users", {}, enableOtp.dump(), "application/json");
    ASSERT_TRUE(anonymous != nullptr);
    EXPECT_EQ(anonymous->status, 401);

    setAuthMethod(*client, adminToken, "password", false);
    auto blocked = client->Post("/api/v1/auth/test_users/login", login.dump(), "application/json");
    ASSERT_TRUE(blocked != nullptr);
    EXPECT_EQ(blocked->status, 403);
    EXPECT_NE(nlohmann::json::parse(blocked->body)["error"].get<std::string>().find("disabled"),
              std::string::npos);

    auto oauthOff = client->Get("/api/v1/auth/test_users/oauth/authorize/google");
    ASSERT_TRUE(oauthOff != nullptr);
    EXPECT_EQ(oauthOff->status, 403);

    setAuthMethod(*client, adminToken, "oauth", true);
    auto oauthOn = client->Get("/api/v1/auth/test_users/oauth/authorize/google");
    ASSERT_TRUE(oauthOn != nullptr);
    EXPECT_NE(oauthOn->status, 403);

    setAuthMethod(*client, adminToken, "otp", true);
    auto after = client->Get("/api/v1/auth/test_users/methods");
    ASSERT_TRUE(after != nullptr);
    const auto updated = nlohmann::json::parse(after->body)["data"];
    EXPECT_FALSE(updated["password"].get<bool>());
    EXPECT_TRUE(updated["otp"].get<bool>());
    EXPECT_TRUE(updated["oauth"].get<bool>());

    const TestHttp::Headers adminHeaders = {{"Authorization", "Bearer " + adminToken}};
    const nlohmann::json baseSchema = {
        {"name", "test_plain"},
        {"type", "base"},
        {"fields", nlohmann::json::array({{{"name", "title"}, {"type", "string"}}})}
    };
    auto created = client->Post("/api/v1/schemas", adminHeaders, baseSchema.dump(), "application/json");
    ASSERT_TRUE(created != nullptr);
    ASSERT_EQ(created->status, 201);
    auto rejected = client->Patch("/api/v1/schemas/test_plain", adminHeaders, enableOtp.dump(), "application/json");
    ASSERT_TRUE(rejected != nullptr);
    EXPECT_EQ(rejected->status, 400);
    client->Delete("/api/v1/schemas/test_plain", adminHeaders);
}

TEST_F(IntegrationAuthTest, OtpRequestRequiresSmtp) {
    MockSmtp smtp(mantis());
    EXPECT_FALSE(mantis().mailer().isConfigured());

    const nlohmann::json body = {{"email", "testuser@example.com"}};
    auto disabled = client->Post("/api/v1/auth/test_users/otp/request", body.dump(), "application/json");
    ASSERT_TRUE(disabled != nullptr);
    EXPECT_EQ(disabled->status, 403);

    setAuthMethod(*client, adminToken, "otp", true);

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
    setAuthMethod(*client, adminToken, "otp", true);
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
    setAuthMethod(*client, adminToken, "otp", true);
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

namespace {

bool jsonFlag(const nlohmann::json &value) {
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_integer())
        return value.get<int>() != 0;
    return false;
}

std::string tokenFromVerificationMail(const std::string &body) {
    const auto pos = body.find("token=");
    if (pos == std::string::npos)
        return {};
    const auto start = pos + 6;
    const auto end = body.find_first_of(" \r\n&\"'<>", start);
    return body.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

class LegacyAuthTable {
public:
    explicit LegacyAuthTable(mb::MantisBase &app) : app_(app) {}
    ~LegacyAuthTable() {
        try {
            const auto sql = app_.db().session();
            *sql << "DROP TABLE IF EXISTS legacy_users";
            *sql << "DELETE FROM mb_tables WHERE id = 'legacy_schema'";
        } catch (...) {
        }
    }

private:
    mb::MantisBase &app_;
};

} // namespace

TEST_F(IntegrationAuthTest, EmailVerifiedFlagGatesLoginWhenRequired) {
    EXPECT_FALSE(mantis().entity("mb_admins").field("email_verified").has_value());
    EXPECT_TRUE(mantis().entity("test_users").field("email_verified").has_value());

    nlohmann::json login = {
        {"identity", "testuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };
    auto loginRes = client->Post("/api/v1/auth/test_users/login", login.dump(), "application/json");
    ASSERT_TRUE(loginRes != nullptr);
    ASSERT_EQ(loginRes->status, 200);
    const auto loggedIn = nlohmann::json::parse(loginRes->body)["data"];
    const auto userId = loggedIn["user"]["id"].get<std::string>();
    EXPECT_FALSE(jsonFlag(loggedIn["user"]["email_verified"]));
    const auto userToken = loggedIn["token"].get<std::string>();

    const TestHttp::Headers user = {{"Authorization", "Bearer " + userToken}};
    const TestHttp::Headers admin = {{"Authorization", "Bearer " + adminToken}};
    const nlohmann::json claimVerified = {{"name", "Test User"}, {"email_verified", true}};
    auto self = client->Patch("/api/v1/entities/test_users/" + userId, user, claimVerified.dump(), "application/json");
    ASSERT_TRUE(self != nullptr);
    ASSERT_EQ(self->status, 200) << self->body;
    EXPECT_FALSE(jsonFlag(nlohmann::json::parse(self->body)["data"]["email_verified"]));

    auto moved = client->Patch("/api/v1/entities/test_users/" + userId, user,
                               nlohmann::json{{"email", "moved@example.com"}}.dump(), "application/json");
    ASSERT_TRUE(moved != nullptr);
    ASSERT_EQ(moved->status, 200) << moved->body;
    EXPECT_FALSE(jsonFlag(nlohmann::json::parse(moved->body)["data"]["email_verified"]));

    auto restored = client->Patch("/api/v1/entities/test_users/" + userId, user,
                                  nlohmann::json{{"email", "testuser@example.com"}}.dump(), "application/json");
    ASSERT_TRUE(restored != nullptr);
    ASSERT_EQ(restored->status, 200) << restored->body;

    setAuthMethod(*client, adminToken, "requireEmailVerification", true);
    auto blocked = client->Post("/api/v1/auth/test_users/login", login.dump(), "application/json");
    ASSERT_TRUE(blocked != nullptr);
    EXPECT_EQ(blocked->status, 403);
    EXPECT_NE(nlohmann::json::parse(blocked->body)["error"].get<std::string>().find("Verify your email"),
              std::string::npos);

    auto marked = client->Patch("/api/v1/entities/test_users/" + userId, admin,
                                nlohmann::json{{"email_verified", true}}.dump(), "application/json");
    ASSERT_TRUE(marked != nullptr);
    ASSERT_EQ(marked->status, 200) << marked->body;
    EXPECT_TRUE(jsonFlag(nlohmann::json::parse(marked->body)["data"]["email_verified"]));

    auto allowed = client->Post("/api/v1/auth/test_users/login", login.dump(), "application/json");
    ASSERT_TRUE(allowed != nullptr);
    EXPECT_EQ(allowed->status, 200);

    auto cleared = client->Patch("/api/v1/entities/test_users/" + userId, admin,
                                 nlohmann::json{{"email_verified", false}}.dump(), "application/json");
    ASSERT_TRUE(cleared != nullptr);
    ASSERT_EQ(cleared->status, 200);

    auto unconfigured = client->Post("/api/v1/auth/test_users/verify-email/request",
                                     nlohmann::json{{"email", "testuser@example.com"}}.dump(), "application/json");
    ASSERT_TRUE(unconfigured != nullptr);
    EXPECT_EQ(unconfigured->status, 400);

    MockSmtp smtp(mantis());
    smtp.enable();
    const auto previous_domain = mantis().settings().configs().value("siteDomain", nlohmann::json(""));
    mantis().settings().setScriptingValue("siteDomain", "http://verify.example");

    nlohmann::json created_body = {
        {"name", "New User"},
        {"email", "newuser@example.com"},
        {"password", TestConfig::getTestPassword()},
        {"email_verified", true}
    };
    auto created = client->Post("/api/v1/entities/test_users", created_body.dump(), "application/json");
    ASSERT_TRUE(created != nullptr);
    ASSERT_EQ(created->status, 201) << created->body;
    const auto created_user = nlohmann::json::parse(created->body)["data"];
    EXPECT_FALSE(jsonFlag(created_user["email_verified"]));
    ASSERT_EQ(mantis().mailer().sentMessages().size(), 1u);
    const auto token = tokenFromVerificationMail(mantis().mailer().sentMessages().front().body_text);
    ASSERT_FALSE(token.empty());
    EXPECT_EQ(created->body.find(token), std::string::npos);

    auto unknown = client->Post("/api/v1/auth/test_users/verify-email/request",
                                nlohmann::json{{"email", "nobody@example.com"}}.dump(), "application/json");
    ASSERT_TRUE(unknown != nullptr);
    EXPECT_EQ(unknown->status, 200);
    EXPECT_EQ(mantis().mailer().sentMessages().size(), 1u);

    auto confirm = client->Get("/api/v1/auth/test_users/verify-email?token=" + token);
    ASSERT_TRUE(confirm != nullptr);
    EXPECT_EQ(confirm->status, 200);
    EXPECT_NE(confirm->body.find("Email verified"), std::string::npos);

    auto again = client->Post("/api/v1/auth/test_users/verify-email",
                              nlohmann::json{{"token", token}}.dump(), "application/json");
    ASSERT_TRUE(again != nullptr);
    EXPECT_EQ(again->status, 400);

    nlohmann::json new_login = {
        {"identity", "newuser@example.com"},
        {"password", TestConfig::getTestPassword()}
    };
    auto new_session = client->Post("/api/v1/auth/test_users/login", new_login.dump(), "application/json");
    ASSERT_TRUE(new_session != nullptr);
    EXPECT_EQ(new_session->status, 200);
    EXPECT_TRUE(jsonFlag(nlohmann::json::parse(new_session->body)["data"]["user"]["email_verified"]));

    mantis().settings().setScriptingValue("siteDomain", previous_domain);

    LegacyAuthTable cleanup(mantis());
    {
        const auto sql = mantis().db().session();
        *sql << "CREATE TABLE legacy_users (id TEXT PRIMARY KEY, email TEXT)";
        *sql << "INSERT INTO legacy_users (id, email) VALUES ('legacy1', 'old@example.com')";
        const nlohmann::json legacy_schema = {
            {"name", "legacy_users"},
            {"type", "auth"},
            {"system", false},
            {"fields", nlohmann::json::array({{{"name", "id"}, {"type", "string"}}})}
        };
        *sql << "INSERT INTO mb_tables (id, schema, created, updated) "
                "VALUES ('legacy_schema', :schema, datetime('now'), datetime('now'))",
                soci::use(legacy_schema);
    }
    ASSERT_TRUE(mantis().db().createSysTables());
    {
        const auto sql = mantis().db().session();
        int verified = -1;
        *sql << "SELECT email_verified FROM legacy_users WHERE id = 'legacy1'", soci::into(verified);
        EXPECT_EQ(verified, 0);
        nlohmann::json stored;
        *sql << "SELECT schema FROM mb_tables WHERE id = 'legacy_schema'", soci::into(stored);
        bool found = false;
        for (const auto &field: stored["fields"]) {
            if (field.value("name", "") == "email_verified")
                found = true;
        }
        EXPECT_TRUE(found);
    }
}
