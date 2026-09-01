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

#else

TEST(ScriptingDisabledAtCompileTime, Skipped) {
    GTEST_SKIP() << "MB_SCRIPTING_ENABLED=OFF — scripting integration tests not built";
}

#endif // MB_SCRIPTING_ENABLED
