#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include "../test_fixure.h"

class IntegrationSchemaTest : public ::testing::Test {
protected:
    void SetUp() override {
        client = std::make_unique<httplib::Client>("http://localhost", 7075);
        
        // Wait for server to be ready
        int retries = 50;
        bool serverReady = false;
        for (int i = 0; i < retries; ++i) {
            if (auto res = client->Get("/api/v1/health"); res && res->status == 200) {
                serverReady = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        ASSERT_TRUE(serverReady);
        
        // Create admin token
        adminToken = createAdminToken();
        ASSERT_FALSE(adminToken.empty());
    }

    void TearDown() override {
        // Clean up test schemas
        if (!adminToken.empty()) {
            httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
            client->Delete("/api/v1/schemas/test_schema", headers);
            client->Delete("/api/v1/schemas/test_schema_update", headers);
        }
    }

    std::string createAdminToken() {
        nlohmann::json setupAdmin = {
            {"email", "admin@test.com"},
            {"password", "adminpass123"}
        };
        
        auto setupRes = client->Post("/api/v1/auth/setup/admin", 
            setupAdmin.dump(), "application/json");
        
        nlohmann::json login = {
            {"entity", "mb_admins"},
            {"identity", "admin@test.com"},
            {"password", "adminpass123"}
        };
        
        auto loginRes = client->Post("/api/v1/auth/login", 
            login.dump(), "application/json");
        
        if (loginRes && loginRes->status == 200) {
            auto response = nlohmann::json::parse(loginRes->body);
            if (response.contains("token")) {
                return response["token"].get<std::string>();
            }
        }
        
        return "";
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
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}, {"required", true}},
            {{"name", "content"}, {"type", "string"}},
            {{"name", "views"}, {"type", "number"}}
        })}
    };
    
    auto res = client->Post("/api/v1/schemas", headers, 
        schema.dump(), "application/json");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);
    
    auto response = nlohmann::json::parse(res->body);
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
    auto res = client->Post("/api/v1/schemas", 
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
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}}
        })}
    };
    
    client->Post("/api/v1/schemas", headers, 
        schema.dump(), "application/json");
    
    // Get schema
    auto res = client->Get("/api/v1/schemas/test_schema", headers);
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = nlohmann::json::parse(res->body);
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
    EXPECT_GT(response["data"].size(), 0); // At least system schemas
}

TEST_F(IntegrationSchemaTest, UpdateSchema) {
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    
    // Create schema first
    nlohmann::json schema = {
        {"name", "test_schema_update"},
        {"type", "base"},
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}}
        })}
    };
    
    client->Post("/api/v1/schemas", headers, 
        schema.dump(), "application/json");
    
    // Update schema
    nlohmann::json updates = {
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}},
            {{"name", "description"}, {"type", "string"}}
        })}
    };
    
    auto res = client->Patch("/api/v1/schemas/test_schema_update", headers,
        updates.dump(), "application/json");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = nlohmann::json::parse(res->body);
    EXPECT_EQ(response["data"]["fields"].size(), 2);
}

TEST_F(IntegrationSchemaTest, DeleteSchema) {
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    
    // Create schema first
    nlohmann::json schema = {
        {"name", "test_schema_delete"},
        {"type", "base"},
        {"fields", nlohmann::json::array()}
    };
    
    client->Post("/api/v1/schemas", headers, 
        schema.dump(), "application/json");
    
    // Delete schema
    auto res = client->Delete("/api/v1/schemas/test_schema_delete", headers);
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    // Verify it's deleted
    auto getRes = client->Get("/api/v1/schemas/test_schema_delete", headers);
    EXPECT_EQ(getRes->status, 404);
}

TEST_F(IntegrationSchemaTest, SchemaWithAccessRules) {
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    
    nlohmann::json schema = {
        {"name", "test_schema_rules"},
        {"type", "base"},
        {"list", {{"mode", "public"}}},
        {"get", {{"mode", "auth"}}},
        {"add", {{"mode", "custom"}, {"expr", "auth.id != null"}}},
        {"update", {{"mode", ""}}}, // Admin only
        {"delete", {{"mode", "custom"}, {"expr", "auth.entity == 'mb_admins'"}}},
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}}
        })}
    };
    
    auto res = client->Post("/api/v1/schemas", headers, 
        schema.dump(), "application/json");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);
    
    auto response = nlohmann::json::parse(res->body);
    EXPECT_EQ(response["data"]["list"]["mode"], "public");
    EXPECT_EQ(response["data"]["add"]["mode"], "custom");
    EXPECT_EQ(response["data"]["add"]["expr"], "auth.id != null");
    
    // Cleanup
    client->Delete("/api/v1/schemas/test_schema_rules", headers);
}

