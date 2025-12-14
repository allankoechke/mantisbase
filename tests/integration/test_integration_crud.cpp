#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include "../test_fixure.h"

class IntegrationCRUDTest : public ::testing::Test {
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
        
        // Create admin token for schema operations
        adminToken = createAdminToken();
        
        // Create test entity schema
        createTestEntity();
    }

    void TearDown() override {
        // Clean up test entity
        if (!adminToken.empty()) {
            httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
            client->Delete("/api/v1/schemas/test_products", headers);
        }
    }

    std::string createAdminToken() {
        // First, try to setup admin if it doesn't exist
        nlohmann::json setupAdmin = {
            {"email", "admin@test.com"},
            {"password", "adminpass123"}
        };
        
        auto setupRes = client->Post("/api/v1/auth/setup/admin", 
            setupAdmin.dump(), "application/json");
        
        // If setup fails, try to login
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

    void createTestEntity() {
        if (adminToken.empty()) return;
        
        httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
        
        nlohmann::json schema = {
            {"name", "test_products"},
            {"type", "base"},
            {"list", {{"mode", "public"}}},
            {"get", {{"mode", "public"}}},
            {"add", {{"mode", "auth"}}},
            {"update", {{"mode", "auth"}}},
            {"delete", {{"mode", ""}}}, // Admin only
            {"fields", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}, {"required", true}},
                {{"name", "price"}, {"type", "number"}, {"required", true}},
                {{"name", "description"}, {"type", "string"}}
            })}
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
            {"fields", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}, {"required", true}},
                {{"name", "email"}, {"type", "string"}, {"required", true}, {"unique", true}},
                {{"name", "password"}, {"type", "string"}, {"required", true}}
            })}
        };
        
        client->Post("/api/v1/schemas", headers, userSchema.dump(), "application/json");
        
        // Create user
        nlohmann::json user = {
            {"name", "Test User"},
            {"email", "user@test.com"},
            {"password", "userpass123"}
        };
        
        auto createRes = client->Post("/api/v1/entities/test_users", 
            user.dump(), "application/json");
        
        if (!createRes || createRes->status != 201) {
            return "";
        }
        
        // Login
        nlohmann::json login = {
            {"entity", "test_users"},
            {"identity", "user@test.com"},
            {"password", "userpass123"}
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

TEST_F(IntegrationCRUDTest, CreateRecord) {
    nlohmann::json record = {
        {"name", "Test Product"},
        {"price", 29.99},
        {"description", "A test product"}
    };
    
    auto res = client->Post("/api/v1/entities/test_products", 
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
    auto res = client->Get("/api/v1/entities/test_products");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].is_array());
    EXPECT_GT(response["data"].size(), 0);
}

TEST_F(IntegrationCRUDTest, GetRecord) {
    // Create a record
    nlohmann::json record = {
        {"name", "Get Test Product"},
        {"price", 39.99}
    };
    
    auto createRes = client->Post("/api/v1/entities/test_products", 
        record.dump(), "application/json");
    
    ASSERT_TRUE(createRes != nullptr);
    auto createResponse = nlohmann::json::parse(createRes->body);
    std::string recordId = createResponse["data"]["id"];
    
    // Get the record
    auto getRes = client->Get(("/api/v1/entities/test_products/" + recordId).c_str());
    
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
    
    auto updateRes = client->Patch(("/api/v1/entities/test_products/" + recordId).c_str(),
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
        record.dump(), "application/json");
    
    ASSERT_TRUE(createRes != nullptr);
    auto createResponse = nlohmann::json::parse(createRes->body);
    std::string recordId = createResponse["data"]["id"];
    
    // Delete the record (requires admin)
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    auto deleteRes = client->Delete(("/api/v1/entities/test_products/" + recordId).c_str(),
        headers);
    
    ASSERT_TRUE(deleteRes != nullptr);
    EXPECT_EQ(deleteRes->status, 200);
    
    // Verify it's deleted
    auto getRes = client->Get(("/api/v1/entities/test_products/" + recordId).c_str());
    EXPECT_EQ(getRes->status, 404);
}

