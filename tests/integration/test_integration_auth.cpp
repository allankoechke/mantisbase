#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include "../test_fixure.h"

class IntegrationAuthTest : public ::testing::Test {
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
        
        // Create admin token for setup
        adminToken = createAdminToken();
        
        // Create test user entity
        createUserEntity();
    }

    void TearDown() override {
        // Clean up test entities
        if (!adminToken.empty()) {
            httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
            client->Delete("/api/v1/schemas/test_users", headers);
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

    void createUserEntity() {
        if (adminToken.empty()) return;
        
        httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
        
        nlohmann::json schema = {
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
        
        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
        
        // Create a test user
        nlohmann::json user = {
            {"name", "Test User"},
            {"email", "testuser@example.com"},
            {"password", "testpass123"}
        };
        
        client->Post("/api/v1/entities/test_users", 
            user.dump(), "application/json");
    }

    std::unique_ptr<httplib::Client> client;
    std::string adminToken;
};

TEST_F(IntegrationAuthTest, LoginWithValidCredentials) {
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", "testpass123"}
    };
    
    auto res = client->Post("/api/v1/auth/login", 
        login.dump(), "application/json");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
    
    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("token"));
    EXPECT_FALSE(response["token"].get<std::string>().empty());
    EXPECT_TRUE(response.contains("user"));
    EXPECT_EQ(response["user"]["email"], "testuser@example.com");
}

TEST_F(IntegrationAuthTest, LoginWithInvalidPassword) {
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", "wrongpassword"}
    };
    
    auto res = client->Post("/api/v1/auth/login", 
        login.dump(), "application/json");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 404);
    
    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("error"));
}

TEST_F(IntegrationAuthTest, LoginWithInvalidEntity) {
    nlohmann::json login = {
        {"entity", "nonexistent"},
        {"identity", "testuser@example.com"},
        {"password", "testpass123"}
    };
    
    auto res = client->Post("/api/v1/auth/login", 
        login.dump(), "application/json");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);
}

TEST_F(IntegrationAuthTest, LoginWithNonAuthEntity) {
    if (adminToken.empty()) {
        GTEST_SKIP() << "Admin token not available";
    }
    
    httplib::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    
        // Create a non-auth entity
        nlohmann::json schema = {
            {"name", "test_products"},
            {"type", "base"},
            {"fields", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}}
            })}
        };
        
        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    
    nlohmann::json login = {
        {"entity", "test_products"},
        {"identity", "test"},
        {"password", "test"}
    };
    
    auto res = client->Post("/api/v1/auth/login", 
        login.dump(), "application/json");
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 400);
    
    // Cleanup
    client->Delete("/api/v1/schemas/test_products", headers);
}

TEST_F(IntegrationAuthTest, RefreshToken) {
    // First login
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", "testpass123"}
    };
    
    auto loginRes = client->Post("/api/v1/auth/login", 
        login.dump(), "application/json");
    
    ASSERT_TRUE(loginRes != nullptr);
    auto loginResponse = nlohmann::json::parse(loginRes->body);
    std::string token = loginResponse["token"];
    
    // Refresh token
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto refreshRes = client->Post("/api/v1/auth/refresh", headers);
    
    ASSERT_TRUE(refreshRes != nullptr);
    EXPECT_EQ(refreshRes->status, 200);
    
    auto refreshResponse = nlohmann::json::parse(refreshRes->body);
    EXPECT_TRUE(refreshResponse.contains("token"));
    EXPECT_NE(refreshResponse["token"].get<std::string>(), token); // Should be different
}

TEST_F(IntegrationAuthTest, RefreshTokenInvalid) {
    httplib::Headers headers = {{"Authorization", "Bearer invalid_token"}};
    auto res = client->Post("/api/v1/auth/refresh", headers);
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 401);
}

TEST_F(IntegrationAuthTest, Logout) {
    // First login
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", "testpass123"}
    };
    
    auto loginRes = client->Post("/api/v1/auth/login", 
        login.dump(), "application/json");
    
    ASSERT_TRUE(loginRes != nullptr);
    auto loginResponse = nlohmann::json::parse(loginRes->body);
    std::string token = loginResponse["token"];
    
    // Logout
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto logoutRes = client->Post("/api/v1/auth/logout", headers);
    
    ASSERT_TRUE(logoutRes != nullptr);
    EXPECT_EQ(logoutRes->status, 200);
}

TEST_F(IntegrationAuthTest, UseTokenForAuthenticatedRequest) {
    // Login
    nlohmann::json login = {
        {"entity", "test_users"},
        {"identity", "testuser@example.com"},
        {"password", "testpass123"}
    };
    
    auto loginRes = client->Post("/api/v1/auth/login", 
        login.dump(), "application/json");
    
    ASSERT_TRUE(loginRes != nullptr);
    auto loginResponse = nlohmann::json::parse(loginRes->body);
    std::string token = loginResponse["token"];
    
    // Use token for authenticated request
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto res = client->Get("/api/v1/entities/test_users", headers);
    
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);
}

