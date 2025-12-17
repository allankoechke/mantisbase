#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "../test_fixure.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"

class IntegrationSchemaTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Server is already started in main.cpp, just get client
        auto &fixture = TestFixture::instance(mb::json{});
        client = std::make_unique<httplib::Client>(fixture.client());

        // Create admin token
        adminToken = TestHelpers::createTestAdminToken(*client);
        ASSERT_FALSE(adminToken.empty());
    }

    void TearDown() override {
        // Clean up test schemas
        TestHelpers::cleanupTestEntity(*client, "test_schema", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_schema_update", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_schema_delete", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_schema_rules", adminToken);
    }

    std::unique_ptr<httplib::Client> client;
    std::string adminToken;
};

TEST_F(IntegrationSchemaTest, CreateSchema) {
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    nlohmann::json schema = {
        {"name", "test_schema"},
        {"type", "base"},
        {"list", {{"mode", "public"}}},
        {"get", {{"mode", "public"}}},
        {"add", {{"mode", "auth"}}},
        {"update", {{"mode", "auth"}}},
        {"delete", {{"mode", ""}}},
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
    std::cout << "Res: " << response.dump() << std::endl;
    EXPECT_TRUE(response.contains("data"));
    EXPECT_EQ(response["data"]["name"], "test_schema");
    EXPECT_EQ(response["data"]["type"], "base");
}

TEST_F(IntegrationSchemaTest, CreateSchemaRequiresAdmin) {
    nlohmann::json schema = {
        {"name", "test_schema_unauth"},
        {"type", "base"},
        {"fields", nlohmann::json::array()}
    };

    // Try without auth
    auto res = client->Post("/api/v1/schemas", {{"Authorization", "Bearer " + adminToken}},
                            schema.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401); // Unauthorized
}

TEST_F(IntegrationSchemaTest, GetSchema) {
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    // Create schema first
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

    // Get schema
    auto res = client->Get("/api/v1/schemas/test_schema", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    std::cout << "RES: " << response.dump() << std::endl;
    std::cout << "RES: " << response["data"]["name"].dump() << std::endl;
    std::cout << "Has Fields? " << response["data"].contains("fields") << std::endl;

    EXPECT_EQ(response["data"]["name"], "test_schema");
    EXPECT_TRUE(response["data"].contains("fields"));
}

TEST_F(IntegrationSchemaTest, ListSchemas) {
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    auto res = client->Get("/api/v1/schemas", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].is_array());
    EXPECT_EQ(response["data"].size(), 0);
}

TEST_F(IntegrationSchemaTest, UpdateSchema) {
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    // Create schema first
    nlohmann::json schema = {
        {"name", "test_schema_update"},
        {"type", "base"},
        {
            "fields", nlohmann::json::array({
                {{"name", "title"}, {"type", "string"}}
            })
        }
    };

    client->Post("/api/v1/schemas", headers,
                 schema.dump(), "application/json");

    // Update schema
    nlohmann::json updates = {
        {
            "fields", nlohmann::json::array({
                {{"name", "description"}, {"type", "string"}}
            })
        }
    };

    auto res = client->Patch("/api/v1/schemas/test_schema_update", headers,
                             updates.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    std::cout << "RES: " << response.dump() << std::endl;
    EXPECT_GT(response["data"]["fields"]["fields"].size(), 2);
}

TEST_F(IntegrationSchemaTest, DeleteSchema) {
    const httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    // Create schema first
    const nlohmann::json schema = {
        {"name", "test_schema_delete"},
        {"type", "base"},
        {"fields", nlohmann::json::array()}
    };

    client->Post("/api/v1/schemas", headers,
                 schema.dump(), "application/json");

    // Delete schema
    auto res = client->Delete("/api/v1/schemas/test_schema_delete", headers);

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 204);

    // Verify it's deleted
    auto getRes = client->Get("/api/v1/schemas/test_schema_delete", headers);
    EXPECT_EQ(getRes->status, 404);
}

TEST_F(IntegrationSchemaTest, SchemaWithAccessRules) {
    const httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema = {
        {"name", "test_schema_rules"},
        {"type", "base"},
        {
            "rules",
            {"list", {{"mode", "public"}}},
            {"get", {{"mode", "auth"}}},
            {"add", {{"mode", "custom"}, {"expr", "auth.id != null"}}},
            {"update", {{"mode", ""}}}, // Admin only
            {"delete", {{"mode", "custom"}, {"expr", "auth.entity == 'mb_admins'"}}},
        },
        {
            "fields", nlohmann::json::array({
                {{"name", "title"}, {"type", "string"}}
            })
        }
    };

    std::cout << "SCHEMA: " << schema.dump() << std::endl;

    auto res = client->Post("/api/v1/schemas", headers,
                            schema.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);

    auto response = nlohmann::json::parse(res->body);
    std::cout << "RES: " << response["data"]["schema"]["rules"].dump() << std::endl;
    EXPECT_EQ(response["data"]["schema"]["rules"]["list"]["mode"], "public");
    EXPECT_EQ(response["data"]["schema"]["rules"]["add"]["mode"], "custom");
    EXPECT_EQ(response["data"]["schema"]["rules"]["add"]["expr"], "auth.id != null");

    // Cleanup
    client->Delete("/api/v1/schemas/test_schema_rules", headers);
}
