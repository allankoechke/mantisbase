#ifndef MANTISBASE_TEST_HELPERS_H
#define MANTISBASE_TEST_HELPERS_H

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include "../include/mantisbase/mantis.h"
#include "../include/mantisbase/core/auth.h"
#include "test_config.h"

namespace TestHelpers {
    /**
     * @brief Wait for server to be ready with exponential backoff
     */
    inline bool waitForServer(httplib::Client& client, int max_retries = 20, int initial_delay_ms = 50) {
        int delay_ms = initial_delay_ms;
        for (int i = 0; i < max_retries; ++i) {
            if (auto res = client.Get("/api/v1/health"); res && res->status == 200) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            // Exponential backoff, max 500ms
            delay_ms = std::min(delay_ms * 2, 500);
        }
        return false;
    }
    
    /**
     * @brief Create test admin token
     * 
     * Creates an admin user directly using MantisBase entity API (like CLI does),
     * then logs in to get a token. This avoids needing the setup endpoint token.
     */
    inline std::string createTestAdminToken(httplib::Client& client) {
        try {
            // Create admin directly using MantisBase entity API (like CLI: mantisbase admins --add)
            // This is simpler than using the setup endpoint which requires a service account token
            auto& app = mb::MantisBase::instance();
            auto admin_entity = app.entity("mb_admins");
            
            // Check if admin already exists
            auto existing = admin_entity.queryFromCols(TestConfig::getAdminEmail(), {"id", "email"});
            if (!existing.has_value()) {
                // Create new admin user directly
                nlohmann::json admin_data = {
                    {"email", TestConfig::getAdminEmail()},
                    {"password", TestConfig::getTestPassword()}
                };
                auto r = admin_entity.create(admin_data);
                // std::cout << "Created record: " << r.dump() << std::endl;
            }
            
            // Login to get token
            nlohmann::json login = {
                {"entity", "mb_admins"},
                {"identity", TestConfig::getAdminEmail()},
                {"password", TestConfig::getTestPassword()}
            };
            
            auto loginRes = client.Post("/api/v1/auth/login",
                login.dump(), "application/json");
            
            if (loginRes && loginRes->status == 200) {
                auto response = nlohmann::json::parse(loginRes->body);
                if (response.contains("data") && response["data"].contains("token")) {
                    return response["data"]["token"].get<std::string>();
                }
            }
        } catch (const std::exception& e) {
            // Fallback: try using setup endpoint with service account token if direct creation fails
            // This requires generating a service account token like openBrowserOnStart() does
            try {
                auto& app = mb::MantisBase::instance();
                
                // Create service account and token (like openBrowserOnStart does)
                auto s_acc = app.entity("mb_service_acc");
                auto record = s_acc.create({});
                
                nlohmann::json claims;
                claims["id"] = record["id"];
                claims["entity"] = "mb_service_acc";
                auto token = mb::Auth::createToken(claims, 30 * 60);
                
                // Use setup endpoint with service account token
                httplib::Headers headers = {{"Authorization", "Bearer " + token}};
                nlohmann::json setupAdmin = {
                    {"email", TestConfig::getAdminEmail()},
                    {"password", TestConfig::getTestPassword()}
                };
                
                client.Post("/api/v1/auth/setup/admin", headers,
                    setupAdmin.dump(), "application/json");
                
                // Login to get token
                nlohmann::json login = {
                    {"entity", "mb_admins"},
                    {"identity", TestConfig::getAdminEmail()},
                    {"password", TestConfig::getTestPassword()}
                };
                
                auto loginRes = client.Post("/api/v1/auth/login",
                    login.dump(), "application/json");
                
                if (loginRes && loginRes->status == 200) {
                    auto response = nlohmann::json::parse(loginRes->body);
                    if (response.contains("data") && response["data"].contains("token")) {
                        return response["data"]["token"].get<std::string>();
                    }
                }
            } catch (...) {
                // Both methods failed
            }
        }
        
        return "";
    }
    
    /**
     * @brief Cleanup test entity
     */
    inline void cleanupTestEntity(httplib::Client& client, const std::string& entity_name, 
                                  const std::string& token) {
        if (token.empty() || entity_name.empty()) return;
        
        httplib::Headers headers = {{"Authorization", "Bearer " + token}};
        client.Delete("/api/v1/schemas/" + entity_name, headers);
    }
    
    /**
     * @brief Generate test user data
     */
    inline nlohmann::json generateTestUser(const std::string& email_prefix = "testuser") {
        std::string unique_id = mb::generateShortId(8);
        return {
            {"email", email_prefix + "_" + unique_id + "@test.com"},
            {"password", TestConfig::getTestPassword()},
            {"name", "Test User " + unique_id}
        };
    }
    
    /**
     * @brief Create test entity schema
     */
    inline bool createTestEntity(httplib::Client& client, const std::string& entity_name,
                                 const std::string& entity_type, const std::string& token,
                                 const nlohmann::json& access_rules = nlohmann::json::object()) {
        if (token.empty()) return false;
        
        httplib::Headers headers = {{"Authorization", "Bearer " + token}};
        
        nlohmann::json schema = {
            {"name", entity_name},
            {"type", entity_type}
        };
        
        // Add access rules if provided
        if (!access_rules.empty()) {
            if (access_rules.contains("list")) schema["list"] = access_rules["list"];
            if (access_rules.contains("get")) schema["get"] = access_rules["get"];
            if (access_rules.contains("add")) schema["add"] = access_rules["add"];
            if (access_rules.contains("update")) schema["update"] = access_rules["update"];
            if (access_rules.contains("delete")) schema["delete"] = access_rules["delete"];
        }
        
        auto res = client.Post("/api/v1/schemas", headers, schema.dump(), "application/json");
        return res && (res->status == 201 || res->status == 409); // Created or already exists
    }
}

#endif //MANTISBASE_TEST_HELPERS_H

