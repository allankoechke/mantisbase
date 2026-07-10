#ifndef MANTISBASE_TEST_HELPERS_H
#define MANTISBASE_TEST_HELPERS_H

#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include "../include/mantisbase/mantis.h"
#include "../include/mantisbase/core/auth.h"
#include "test_config.h"
#include "test_http_client.h"

namespace TestHelpers {
    inline bool waitForServer(TestHttp::Client& client, int max_retries = 20, int initial_delay_ms = 50) {
        int delay_ms = initial_delay_ms;
        for (int i = 0; i < max_retries; ++i) {
            if (auto res = client.Get("/api/v1/health"); res && res->status == 200) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms = std::min(delay_ms * 2, 500);
        }
        return false;
    }

    inline std::string createTestAdminToken(TestHttp::Client& client) {
        try {
            auto& app = mb::MantisBase::instance();
            auto admin_entity = app.entity("mb_admins");

            auto existing = admin_entity.queryFromCols(TestConfig::getAdminEmail(), {"id", "email"});
            if (!existing.has_value()) {
                nlohmann::json admin_data = {
                    {"email", TestConfig::getAdminEmail()},
                    {"password", TestConfig::getTestPassword()}
                };
                auto r = admin_entity.create(admin_data);
            }

            nlohmann::json login = {
                {"identity", TestConfig::getAdminEmail()},
                {"password", TestConfig::getTestPassword()}
            };

            auto loginRes = client.Post("/api/v1/sys/admins/login",
                login.dump(), "application/json");

            if (loginRes && loginRes->status == 200) {
                auto response = nlohmann::json::parse(loginRes->body);
                if (response.contains("data") && response["data"].contains("token")) {
                    return response["data"]["token"].get<std::string>();
                }
            }
        } catch (const std::exception& e) {
            try {
                auto& app = mb::MantisBase::instance();

                auto s_acc = app.entity("mb_service_acc");
                auto record = s_acc.create({});

                nlohmann::json claims;
                claims["id"] = record["id"];
                claims["entity"] = "mb_service_acc";
                auto token = mb::Auth::createToken(claims, 30 * 60);

                TestHttp::Headers headers = {{"Authorization", "Bearer " + token}};
                nlohmann::json setupAdmin = {
                    {"email", TestConfig::getAdminEmail()},
                    {"password", TestConfig::getTestPassword()}
                };

                client.Post("/api/v1/sys/admins/setup", headers,
                    setupAdmin.dump(), "application/json");

                nlohmann::json login = {
                    {"identity", TestConfig::getAdminEmail()},
                    {"password", TestConfig::getTestPassword()}
                };

                auto loginRes = client.Post("/api/v1/sys/admins/login",
                    login.dump(), "application/json");

                if (loginRes && loginRes->status == 200) {
                    auto response = nlohmann::json::parse(loginRes->body);
                    if (response.contains("data") && response["data"].contains("token")) {
                        return response["data"]["token"].get<std::string>();
                    }
                }
            } catch (...) {
            }
        }

        return "";
    }

    inline void cleanupTestEntity(TestHttp::Client& client, const std::string& entity_name,
                                  const std::string& token) {
        if (token.empty() || entity_name.empty()) return;

        TestHttp::Headers headers = {{"Authorization", "Bearer " + token}};
        client.Delete("/api/v1/schemas/" + entity_name, headers);
    }

    inline nlohmann::json generateTestUser(const std::string& email_prefix = "testuser") {
        std::string unique_id = mb::generateShortId(8);
        return {
            {"email", email_prefix + "_" + unique_id + "@test.com"},
            {"password", TestConfig::getTestPassword()},
            {"name", "Test User " + unique_id}
        };
    }

    inline bool createTestEntity(TestHttp::Client& client, const std::string& entity_name,
                                 const std::string& entity_type, const std::string& token,
                                 const nlohmann::json& access_rules = nlohmann::json::object()) {
        if (token.empty()) return false;

        TestHttp::Headers headers = {{"Authorization", "Bearer " + token}};

        nlohmann::json schema = {
            {"name", entity_name},
            {"type", entity_type}
        };

        if (!access_rules.empty()) {
            if (access_rules.contains("list")) schema["list"] = access_rules["list"];
            if (access_rules.contains("get")) schema["get"] = access_rules["get"];
            if (access_rules.contains("add")) schema["add"] = access_rules["add"];
            if (access_rules.contains("update")) schema["update"] = access_rules["update"];
            if (access_rules.contains("delete")) schema["delete"] = access_rules["delete"];
        }

        auto res = client.Post("/api/v1/schemas", headers, schema.dump(), "application/json");
        return res && (res->status == 201 || res->status == 409);
    }
}

#endif //MANTISBASE_TEST_HELPERS_H
