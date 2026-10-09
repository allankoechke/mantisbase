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

#else

TEST(ScriptingDisabledAtCompileTime, Skipped) {
    GTEST_SKIP() << "MB_SCRIPTING_ENABLED=OFF — scripting integration tests not built";
}

#endif // MB_SCRIPTING_ENABLED
