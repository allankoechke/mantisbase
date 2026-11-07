#include "../common/test_fixture.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"
#include "../../include/mantisbase/utils/utils.h"

#include <gtest/gtest.h>

namespace {

nlohmann::json parseBody(const TestHttp::Response &res)
{
    return nlohmann::json::parse(res.body);
}

class IntegrationSettingsTest : public MbServerFixture {
protected:
    void SetUp() override
    {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        ASSERT_FALSE(adminToken.empty());
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
};

class IntegrationSettingsGateTest : public MbServerFixture {
protected:
    void SetUp() override
    {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        ASSERT_FALSE(adminToken.empty());
        TestHelpers::setEnvVar("MB_DISABLE_CONFIG_MUTATIONS");
    }

    void TearDown() override
    {
        TestHelpers::setEnvVar("MB_DISABLE_CONFIG_MUTATIONS");
        MbServerFixture::TearDown();
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
};

} // namespace

TEST_F(IntegrationSettingsTest, GetRequiresAdminAuth)
{
    auto res = client->Get("/api/v1/sys/settings/config");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationSettingsTest, GetReturnsDefaultsForAdmin)
{
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    auto res = client->Get("/api/v1/sys/settings/config", headers);
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = parseBody(*res);
    EXPECT_TRUE(body.contains("data"));
    EXPECT_EQ(body["data"]["orgName"].get<std::string>(), "ACME Corp");
    EXPECT_EQ(body["data"]["siteDomain"].get<std::string>(), "https://acme.example.com");
    EXPECT_TRUE(body["data"]["corsAllowedOrigins"].is_array());
    EXPECT_EQ(body["data"]["corsAllowedOrigins"].size(), 2u);
    EXPECT_EQ(body["data"]["maxFileSize"].get<int>(), 10 * 1024 * 1024);
    EXPECT_EQ(body["data"]["logRetentionDays"].get<int>(), 5);
    EXPECT_TRUE(body["data"].contains("mantisVersion"));
    EXPECT_TRUE(body["data"].contains("smtp"));
}

TEST_F(IntegrationSettingsTest, PatchUpdatesConfig)
{
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json patch = {
        {"orgName", "Test Org"},
        {"siteDomain", "https://app.example.com"},
        {"maxFileSize", 20971520}
    };

    auto patchRes = client->Patch("/api/v1/sys/settings/config", headers, patch.dump(), "application/json");
    ASSERT_TRUE(patchRes != nullptr);
    EXPECT_EQ(patchRes->status, 200);

    auto patchBody = parseBody(*patchRes);
    EXPECT_EQ(patchBody["data"]["orgName"].get<std::string>(), "Test Org");
    EXPECT_EQ(patchBody["data"]["maxFileSize"].get<int>(), 20971520);

    auto getRes = client->Get("/api/v1/sys/settings/config", headers);
    ASSERT_TRUE(getRes != nullptr);
    EXPECT_EQ(getRes->status, 200);
    auto getBody = parseBody(*getRes);
    EXPECT_EQ(getBody["data"]["orgName"].get<std::string>(), "Test Org");
}

TEST_F(IntegrationSettingsTest, PatchRejectsInvalidMaxFileSize)
{
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json patch = {{"maxFileSize", 0}};

    auto res = client->Patch("/api/v1/sys/settings/config", headers, patch.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);
}

TEST_F(IntegrationSettingsTest, AdminCreateBlockedWhenRegistrationDisabled)
{
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    auto disableRes = client->Patch("/api/v1/sys/settings/config", headers,
                                    nlohmann::json{{"disableAdminRegistration", true}}.dump(),
                                    "application/json");
    ASSERT_TRUE(disableRes != nullptr);
    ASSERT_EQ(disableRes->status, 200);

    const std::string email = "admin_reg_blocked_" + mb::generateShortId(8) + "@test.com";
    nlohmann::json body = {
        {"email", email},
        {"password", TestConfig::getTestPassword()}
    };
    auto createRes = client->Post("/api/v1/sys/admins", headers, body.dump(), "application/json");
    ASSERT_TRUE(createRes != nullptr);
    EXPECT_EQ(createRes->status, 503);
    EXPECT_EQ(parseBody(*createRes)["error"].get<std::string>(), "This feature has been disabled.");

    auto enableRes = client->Patch("/api/v1/sys/settings/config", headers,
                                   nlohmann::json{{"disableAdminRegistration", false}}.dump(),
                                   "application/json");
    ASSERT_TRUE(enableRes != nullptr);
    EXPECT_EQ(enableRes->status, 200);
}

TEST_F(IntegrationSettingsTest, SchemaMutationsBlockedWhenDisabled)
{
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    auto disableRes = client->Patch("/api/v1/sys/settings/config", headers,
                                    nlohmann::json{{"disableSchemaMutations", true}}.dump(),
                                    "application/json");
    ASSERT_TRUE(disableRes != nullptr);
    ASSERT_EQ(disableRes->status, 200);

    nlohmann::json schema = {
        {"name", "schema_gate_test"},
        {"type", "base"},
        {
            "rules", {
                {"list", {{"mode", "public"}}},
                {"get", {{"mode", "public"}}},
                {"add", {{"mode", "public"}}},
                {"update", {{"mode", "public"}}},
                {"delete", {{"mode", ""}}}
            }
        },
        {"fields", nlohmann::json::array({{{"name", "title"}, {"type", "string"}}})}
    };

    auto createRes = client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    ASSERT_TRUE(createRes != nullptr);
    EXPECT_EQ(createRes->status, 503);
    EXPECT_EQ(parseBody(*createRes)["error"].get<std::string>(), "This feature has been disabled.");

    auto listRes = client->Get("/api/v1/schemas", headers);
    ASSERT_TRUE(listRes != nullptr);
    EXPECT_EQ(listRes->status, 200);

    auto enableRes = client->Patch("/api/v1/sys/settings/config", headers,
                                   nlohmann::json{{"disableSchemaMutations", false}}.dump(),
                                   "application/json");
    ASSERT_TRUE(enableRes != nullptr);
    EXPECT_EQ(enableRes->status, 200);
}

TEST_F(IntegrationSettingsTest, SmtpCredentialsApplyFromApiAndScripting)
{
    struct RestoreSmtp
    {
        mb::MantisBase &app;
        ~RestoreSmtp()
        {
            try
            {
                app.settings().setScriptingValue("smtp", {
                    {"host", ""},
                    {"port", 587},
                    {"user", ""},
                    {"password", ""},
                    {"from", ""},
                    {"tls", true}
                });
            }
            catch (...)
            {
            }
        }
    } restore{mantis()};

    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json patch = {
        {"smtp", {
            {"host", "mock"},
            {"port", 587},
            {"user", "mailer"},
            {"password", "s3cret"},
            {"from", "app@example.com"},
            {"tls", true}
        }}
    };

    auto patchRes = client->Patch("/api/v1/sys/settings/config", headers, patch.dump(), "application/json");
    ASSERT_TRUE(patchRes != nullptr);
    ASSERT_EQ(patchRes->status, 200);
    EXPECT_EQ(parseBody(*patchRes)["data"]["smtp"]["password"].get<std::string>(), "********");

    auto getRes = client->Get("/api/v1/sys/settings/config", headers);
    ASSERT_TRUE(getRes != nullptr);
    ASSERT_EQ(getRes->status, 200);
    EXPECT_EQ(parseBody(*getRes)["data"]["smtp"]["password"].get<std::string>(), "********");

    const auto &mailer = mantis().mailer();
    ASSERT_TRUE(mailer.isConfigured());
    EXPECT_EQ(mailer.config().host, "mock");
    EXPECT_EQ(mailer.config().password, "s3cret");
    EXPECT_EQ(mailer.config().from_address, "app@example.com");
    EXPECT_TRUE(mailer.config().use_tls);

    nlohmann::json keep = {
        {"smtp", {
            {"host", "mock"},
            {"port", 2525},
            {"password", "********"}
        }}
    };
    auto keepRes = client->Patch("/api/v1/sys/settings/config", headers, keep.dump(), "application/json");
    ASSERT_TRUE(keepRes != nullptr);
    ASSERT_EQ(keepRes->status, 200);
    EXPECT_EQ(mailer.config().port, 2525);
    EXPECT_EQ(mailer.config().password, "s3cret");

    nlohmann::json omit = {{"smtp", {{"user", "mailer"}}}};
    auto omitRes = client->Patch("/api/v1/sys/settings/config", headers, omit.dump(), "application/json");
    ASSERT_TRUE(omitRes != nullptr);
    ASSERT_EQ(omitRes->status, 200);
    EXPECT_EQ(mailer.config().password, "s3cret");

    mantis().settings().setScriptingValue("smtp", {
        {"host", "mock"},
        {"port", 587},
        {"user", "mailer"},
        {"password", "other"},
        {"from", "app@example.com"},
        {"tls", true}
    });
    EXPECT_EQ(mailer.config().password, "other");
    EXPECT_EQ(mantis().settings().getScriptingValue("smtp")["password"].get<std::string>(), "********");

    mantis().settings().reloadScriptingConfigs();
    EXPECT_TRUE(mailer.isConfigured());
    EXPECT_EQ(mailer.config().password, "other");
    EXPECT_EQ(mailer.config().host, "mock");
}

TEST_F(IntegrationSettingsGateTest, PatchAllowedWhenGateOpen)
{
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json patch = {{"orgName", "Gate Open Org"}};

    auto res = client->Patch("/api/v1/sys/settings/config", headers, patch.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}

TEST_F(IntegrationSettingsGateTest, PatchBlockedWhenGateClosed)
{
    TestHelpers::setEnvVar("MB_DISABLE_CONFIG_MUTATIONS", "true");
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    nlohmann::json patch = {{"orgName", "Gate Closed Org"}};

    auto res = client->Patch("/api/v1/sys/settings/config", headers, patch.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 503);

    auto body = parseBody(*res);
    EXPECT_EQ(body["error"].get<std::string>(), "Resource action temporarily disabled");
}

TEST_F(IntegrationSettingsGateTest, GetStillAllowedWhenGateClosed)
{
    TestHelpers::setEnvVar("MB_DISABLE_CONFIG_MUTATIONS", "true");
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    auto res = client->Get("/api/v1/sys/settings/config", headers);
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}
