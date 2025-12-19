#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "../test_fixure.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"

class IntegrationCRUDTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Server is already started in main.cpp, just get client
        const auto &fixture = TestFixture::instance(mb::json{});
        client = std::make_unique<httplib::Client>(fixture.client());

        // Create admin token for schema operations
        adminToken = TestHelpers::createTestAdminToken(*client);

        // Create test entity schema
        createTestEntity();
    }

    void TearDown() override {
        // Clean up test entities
        TestHelpers::cleanupTestEntity(*client, "test_products", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_users", adminToken);
    }

    void createTestEntity() {
        if (adminToken.empty()) return;

        const httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        const nlohmann::json schema = {
            {"name", "test_products"},
            {"type", "base"},
            {"list", {{"mode", "public"}}},
            {"get", {{"mode", "public"}}},
            {"add", {{"mode", "auth"}}},
            {"update", {{"mode", "auth"}}},
            {"delete", {{"mode", ""}}}, // Admin only
            {
                "fields", nlohmann::json::array({
                    {{"name", "name"}, {"type", "string"}, {"required", true}},
                    {{"name", "price"}, {"type", "double"}, {"required", true}},
                    {{"name", "description"}, {"type", "string"}}
                })
            }
        };

        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    }

    std::string createUserAndGetToken() {
        if (adminToken.empty()) return "";

        httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        // Create user entity schema first
        nlohmann::json userSchema = {
            {"name", "test_users"},
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

        client->Post("/api/v1/schemas", headers, userSchema.dump(), "application/json");

        // Create user
        nlohmann::json user = {
            {"name", "Test User"},
            {"email", "user@test.com"},
            {"password", TestConfig::getTestPassword()}
        };

        auto createRes = client->Post("/api/v1/entities/test_users", headers,
                                      user.dump(), "application/json");

        if (!createRes || createRes->status != 201) {
            return "";
        }

        // Login
        nlohmann::json login = {
            {"entity", "test_users"},
            {"identity", "user@test.com"},
            {"password", TestConfig::getTestPassword()}
        };

        auto loginRes = client->Post("/api/v1/auth/login",
                                     login.dump(), "application/json");

        if (loginRes && loginRes->status == 200) {
            auto response = nlohmann::json::parse(loginRes->body);
            if (response.contains("data") && response["data"].contains("token")) {
                return response["data"]["token"].get<std::string>();
            }
        }

        return "";
    }

    std::unique_ptr<httplib::Client> client;
    std::string adminToken;
};

TEST_F(IntegrationCRUDTest, CreateRecord) {
    nlohmann::json record = {
        {"name", "Test Product"},
        {"price", 29.99},
        {"description", "A test product"}
    };

    auto res = client->Post("/api/v1/entities/test_products", {{"Authorization", "Bearer " + adminToken}},
                            record.dump(), "application/json");

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].contains("id"));
    EXPECT_EQ(response["data"]["name"], "Test Product");
}

TEST_F(IntegrationCRUDTest, ListRecords) {
    // Create a record first
    nlohmann::json record = {
        {"name", "List Test Product"},
        {"price", 19.99}
    };
    client->Post("/api/v1/entities/test_products",
                 record.dump(), "application/json");

    // List records
    auto res = client->Get("/api/v1/entities/test_products", {{"Authorization", "Bearer " + adminToken}});

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data") && response["data"].contains("items"));
    EXPECT_TRUE(response["data"]["items"].is_array());
    EXPECT_EQ(response["data"]["items"].size(), response["data"]["items_count"]);
}

TEST_F(IntegrationCRUDTest, GetRecord) {
    // Create a record
    nlohmann::json record = {
        {"name", "Get Test Product"},
        {"price", 39.99}
    };

    const httplib::Headers headers{{"Authorization", "Bearer " + adminToken}};

    auto createRes = client->Post("/api/v1/entities/test_products",
                                  headers,
                                  record.dump(), "application/json");

    ASSERT_TRUE(createRes != nullptr);
    ASSERT_TRUE(createRes->status == 201);
    ASSERT_FALSE(createRes->body.empty());

    auto createResponse = nlohmann::json::parse(createRes->body);
    std::string recordId = createResponse["data"]["id"];

    // Get the record
    auto getRes = client->Get("/api/v1/entities/test_products/" + recordId, headers);

    ASSERT_TRUE(getRes != nullptr);
    EXPECT_EQ(getRes->status, 200);

    auto getResponse = nlohmann::json::parse(getRes->body);
    EXPECT_EQ(getResponse["data"]["id"], recordId);
    EXPECT_EQ(getResponse["data"]["name"], "Get Test Product");
}

TEST_F(IntegrationCRUDTest, UpdateRecord) {
    std::string userToken = createUserAndGetToken();
    ASSERT_FALSE(userToken.empty());

    httplib::Headers headers = {{"Authorization", "Bearer " + userToken}};

    // Create a record
    nlohmann::json record = {
        {"name", "Update Test Product"},
        {"price", 49.99}
    };

    auto createRes = client->Post("/api/v1/entities/test_products",
                                  headers, record.dump(), "application/json");

    ASSERT_TRUE(createRes != nullptr);
    auto createResponse = nlohmann::json::parse(createRes->body);
    std::string recordId = createResponse["data"]["id"];

    // Update the record
    nlohmann::json updates = {
        {"name", "Updated Product Name"},
        {"price", 59.99}
    };

    auto updateRes = client->Patch("/api/v1/entities/test_products/" + recordId,
                                   headers, updates.dump(), "application/json");

    ASSERT_TRUE(updateRes != nullptr);
    EXPECT_EQ(updateRes->status, 200);

    auto updateResponse = nlohmann::json::parse(updateRes->body);
    EXPECT_EQ(updateResponse["data"]["name"], "Updated Product Name");
    EXPECT_EQ(updateResponse["data"]["price"], 59.99);
}

TEST_F(IntegrationCRUDTest, DeleteRecord) {
    // Create a record
    nlohmann::json record = {
        {"name", "Delete Test Product"},
        {"price", 99.99}
    };

    auto createRes = client->Post("/api/v1/entities/test_products",
                                  {{"Authorization", "Bearer " + adminToken}},
                                  record.dump(), "application/json");

    ASSERT_TRUE(createRes != nullptr);
    ASSERT_TRUE(createRes->status == 201);
    ASSERT_FALSE(createRes->body.empty());

    auto createResponse = nlohmann::json::parse(createRes->body);
    std::string recordId = createResponse["data"]["id"];

    // Delete the record (requires admin)
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    auto deleteRes = client->Delete("/api/v1/entities/test_products/" + recordId, headers);

    ASSERT_TRUE(deleteRes != nullptr);
    EXPECT_EQ(deleteRes->status, 204);

    // Verify it's deleted
    auto getRes = client->Get("/api/v1/entities/test_products/" + recordId, headers);
    EXPECT_EQ(getRes->status, 404);
}
