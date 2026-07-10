#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "../common/test_environment.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

class IntegrationSchemaTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& fixture = MbTestEnv::instance();
        client = std::make_unique<TestHttp::Client>(fixture.client());

        adminToken = TestHelpers::createTestAdminToken(*client);
        ASSERT_FALSE(adminToken.empty());
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "test_schema", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_schema_update", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_schema_delete", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_schema_rules", adminToken);
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
};

TEST_F(IntegrationSchemaTest, CreateSchema) {
    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema = {
        {"name", "test_schema"},
        {"type", "base"},
        {
            "rules",
            {
                {"list", {{"mode", "public"}}},
                {"get", {{"mode", "public"}}},
                {"add", {{"mode", "auth"}}},
                {"update", {{"mode", "auth"}}},
                {"delete", {{"mode", ""}}}
            },
        },
        {
            "fields", nlohmann::json::array({
                {{"name", "title"}, {"type", "string"}, {"required", true}},
                {{"name", "content"}, {"type", "string"}},
                {{"name", "views"}, {"type", "int32"}}
            })
        }
    };

    auto res = client->Post("/api/v1/schemas", headers,
                            schema.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data"));
    EXPECT_EQ(response["data"]["schema"]["name"], "test_schema");
    EXPECT_EQ(response["data"]["schema"]["type"], "base");
}

TEST_F(IntegrationSchemaTest, CreateSchemaRequiresAdmin) {
    nlohmann::json schema = {
        {"name", "test_schema_unauth"},
        {"type", "base"},
        {"fields", nlohmann::json::array()}
    };

    auto res = client->Post("/api/v1/schemas",
                            schema.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 403);
}

TEST_F(IntegrationSchemaTest, GetSchema) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    nlohmann::json schema = {
        {"name", "test_schema"},
        {"type", "base"},
        {
            "fields", nlohmann::json::array({
                {{"name", "title"}, {"type", "string"}}
            })
        }
    };

    client->Post("/api/v1/schemas", headers,
                 schema.dump(), "application/json");

    auto res = client->Get("/api/v1/schemas/test_schema", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_EQ(response["status"], 200);
    EXPECT_EQ(response["data"]["schema"]["name"], "test_schema");
    EXPECT_TRUE(response["data"]["schema"].contains("fields"));
}

TEST_F(IntegrationSchemaTest, ListSchemas) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    auto res = client->Get("/api/v1/schemas", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].is_array());
    EXPECT_EQ(response["data"].size(), 0);
}

TEST_F(IntegrationSchemaTest, UpdateSchema) {
    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema = {
        {"name", "test_schema_update"},
        {"type", "base"},
        {
            "fields", nlohmann::json::array({
                {{"name", "title"}, {"type", "string"}}
            })
        }
    };

    auto res = client->Post("/api/v1/schemas", headers,
                            schema.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);

    const nlohmann::json updates = {
        {
            "fields", nlohmann::json::array({
                {{"name", "description"}, {"type", "string"}}
            })
        }
    };

    res = client->Patch("/api/v1/schemas/test_schema_update", headers,
                        updates.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_GT(response["data"]["schema"]["fields"].size(), 2);
}

TEST_F(IntegrationSchemaTest, DeleteSchema) {
    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema = {
        {"name", "test_schema_delete"},
        {"type", "base"},
        {"fields", nlohmann::json::array()}
    };

    client->Post("/api/v1/schemas", headers,
                 schema.dump(), "application/json");

    auto res = client->Delete("/api/v1/schemas/test_schema_delete", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 204);

    auto getRes = client->Get("/api/v1/schemas/test_schema_delete", headers);
    EXPECT_EQ(getRes->status, 404);
}

TEST_F(IntegrationSchemaTest, SchemaWithAccessRules) {
    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema = {
        {"name", "test_schema_rules"},
        {"type", "base"},
        {
            "rules", {
                {"list", {{"mode", "public"}}},
                {"get", {{"mode", "auth"}}},
                {"add", {{"mode", "custom"}, {"expr", "auth.id != null"}}},
                {"update", {{"mode", ""}}},
                {"delete", {{"mode", "custom"}, {"expr", "auth.entity == 'mb_admins'"}}},
            },
        },
        {
            "fields", nlohmann::json::array({
                {{"name", "title"}, {"type", "string"}}
            })
        }
    };

    auto res = client->Post("/api/v1/schemas", headers,
                            schema.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_EQ(response["data"]["schema"]["rules"]["list"]["mode"], "public");
    EXPECT_EQ(response["data"]["schema"]["rules"]["add"]["mode"], "custom");
    EXPECT_EQ(response["data"]["schema"]["rules"]["add"]["expr"], "auth.id != null");

    client->Delete("/api/v1/schemas/test_schema_rules", headers);
}
