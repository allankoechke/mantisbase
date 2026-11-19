#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "../common/test_fixture.h"
#include "../common/test_http_client.h"

#ifdef MB_SCRIPTING_ENABLED

namespace {
    nlohmann::json parseBody(const TestHttp::Response &res) {
        return nlohmann::json::parse(res.body);
    }
} // namespace

class IntegrationScriptingTest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
    }

    std::unique_ptr<TestHttp::Client> client;
};

TEST_F(IntegrationScriptingTest, PingRouteReturnsJson) {
    const auto res = client->Get("/api/v1/test/scripting/ping");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.value("pong", false));
    EXPECT_EQ(body.value("source", ""), "test-script");
}

TEST_F(IntegrationScriptingTest, DbQuerySingleRowShape) {
    const auto res = client->Get("/api/v1/test/scripting/settings-count");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.contains("settings_count"));
    EXPECT_TRUE(body["settings_count"].is_number());
}

TEST_F(IntegrationScriptingTest, JsMiddlewareAbortStopsHandler) {
    const auto res = client->Get("/api/v1/test/scripting/mw-abort");
    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_EQ(res->body.find("\"reached\":true"), std::string::npos);
}

TEST_F(IntegrationScriptingTest, CppMiddlewareBlocksUnauthenticatedRequest) {
    const auto res = client->Get("/api/v1/test/scripting/protected");
    ASSERT_TRUE(res);
    EXPECT_NE(res->status, 200);
    EXPECT_EQ(res->body.find("\"protected\":true"), std::string::npos);
}

TEST_F(IntegrationScriptingTest, JsShorthandGetRouteReturnsJson) {
    const auto res = client->Get("/api/v1/test/scripting/shorthand-get");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.value("ok", false));
    EXPECT_EQ(body.value("method", ""), "get");
}

TEST_F(IntegrationScriptingTest, JsShorthandPostRouteReturnsJson) {
    const auto res = client->Post("/api/v1/test/scripting/shorthand-post", "{}", "application/json");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.value("ok", false));
    EXPECT_EQ(body.value("method", ""), "post");
}

TEST_F(IntegrationScriptingTest, JsShorthandPatchRouteReturnsJson) {
    const auto res = client->Patch("/api/v1/test/scripting/shorthand-patch", {}, "{}", "application/json");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.value("ok", false));
    EXPECT_EQ(body.value("method", ""), "patch");
}

TEST_F(IntegrationScriptingTest, JsShorthandPutRouteReturnsJson) {
    const auto res = client->Put("/api/v1/test/scripting/shorthand-put", {}, "{}", "application/json");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.value("ok", false));
    EXPECT_EQ(body.value("method", ""), "put");
}

TEST_F(IntegrationScriptingTest, JsShorthandDeleteRouteReturnsJson) {
    const auto res = client->Delete("/api/v1/test/scripting/shorthand-delete");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200);

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.value("ok", false));
    EXPECT_EQ(body.value("method", ""), "delete");
}

TEST_F(IntegrationScriptingTest, JsRedirectDefaultsTo302WithLocation) {
    const auto res = client->Get("/api/v1/test/scripting/old-path");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 302);
    EXPECT_EQ(res->header("Location"), "/api/v1/test/scripting/shorthand-get");
}

TEST_F(IntegrationScriptingTest, JsRedirectSupportsPermanent301) {
    const auto res = client->Get("/api/v1/test/scripting/old-permanent");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 301);
    EXPECT_EQ(res->header("Location"), "/api/v1/test/scripting/shorthand-get");
}

TEST_F(IntegrationScriptingTest, DbSessionTransactionCommitsAndRollsBack) {
    const auto res = client->Get("/api/v1/test/scripting/db");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;

    const auto body = parseBody(*res);
    EXPECT_TRUE(body.value("connected", false));
    EXPECT_EQ(body.value("backend", ""), "sqlite3");
    EXPECT_TRUE(body.value("committed", false));
    EXPECT_TRUE(body.value("rolledBack", false));
    EXPECT_TRUE(body.value("lastQueryHasInsert", false));
}

TEST_F(IntegrationScriptingTest, HelperBindingsReturnValues) {
    const auto res = client->Get("/api/v1/test/scripting/helpers");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;

    const auto body = parseBody(*res);
    for (const auto &key : {"timeId", "readable", "shortId", "env", "password", "extension",
                            "contentType", "flag", "filename", "logged"}) {
        EXPECT_TRUE(body.value(key, false)) << key;
    }
}

TEST_F(IntegrationScriptingTest, MailerBindingsUseObjects) {
    const auto res = client->Get("/api/v1/test/scripting/mailer");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;

    const auto body = parseBody(*res);
    for (const auto &key : {"configured", "maskKept", "sent", "template", "allowed"}) {
        EXPECT_TRUE(body.value(key, false)) << key;
    }
    EXPECT_TRUE(body.value("appliedPort", false));
    EXPECT_FALSE(mantis().mailer().isConfigured());
}

TEST_F(IntegrationScriptingTest, AuthKeyAndOAuthBindingsUseObjects) {
    const auto res = client->Get("/api/v1/test/scripting/auth");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;

    const auto body = parseBody(*res);
    for (const auto &key : {"tokenVerified", "timeout", "otpSent", "emailSent", "resetSent", "quiet",
                            "keyPrefix", "keyListed", "adminKey", "google", "providersArray"}) {
        EXPECT_TRUE(body.value(key, false)) << key;
    }
    EXPECT_FALSE(mantis().mailer().isConfigured());
}

TEST_F(IntegrationScriptingTest, FileBindingsAcceptObjects) {
    const auto res = client->Get("/api/v1/test/scripting/files");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;

    const auto body = parseBody(*res);
    for (const auto &key : {"dir", "exists", "missing", "removed", "canonical"}) {
        EXPECT_TRUE(body.value(key, false)) << key;
    }
}

TEST_F(IntegrationScriptingTest, RealtimeHooksAndBroadcast) {
    const auto res = client->Get("/api/v1/test/scripting/realtime");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_TRUE(parseBody(*res).value("hooked", false));
}

TEST_F(IntegrationScriptingTest, EnvGateBlocksWhenVariableIsSet) {
    const auto res = client->Get("/api/v1/test/scripting/env-blocked");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 503);
    EXPECT_EQ(res->body.find("\"reached\":true"), std::string::npos);
}

TEST_F(IntegrationScriptingTest, EnvGateAllowsWhenVariableIsUnset) {
    const auto res = client->Get("/api/v1/test/scripting/env-open");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_TRUE(parseBody(*res).value("reached", false));
}

TEST_F(IntegrationScriptingTest, RateLimitBindingLetsDisabledHarnessThrough) {
    const auto res = client->Get("/api/v1/test/scripting/rate-limit");
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, 200) << res->body;
    EXPECT_TRUE(parseBody(*res).value("reached", false));
}

#else

TEST(ScriptingDisabledAtCompileTime, Skipped) {
    GTEST_SKIP() << "MB_SCRIPTING_ENABLED=OFF — scripting integration tests not built";
}

#endif // MB_SCRIPTING_ENABLED
