#ifndef MANTISBASE_TEST_HELPERS_H
#define MANTISBASE_TEST_HELPERS_H

#include <nlohmann/json.hpp>
#include <optional>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "../include/mantisbase/mantis.h"
#include "../include/mantisbase/core/auth.h"
#include "test_config.h"
#include "test_http_client.h"

namespace TestHelpers {
    inline void setEnvVar(const char *key, const std::optional<std::string> &value = std::nullopt) {
#ifdef _WIN32
        if (value.has_value()) {
            _putenv_s(key, value->c_str());
        } else {
            _putenv_s(key, "");
        }
#else
        if (value.has_value()) {
            setenv(key, value->c_str(), 1);
        } else {
            unsetenv(key);
        }
#endif
    }

    inline std::string loginUser(TestHttp::Client &client,
                                 const std::string &entity,
                                 const std::string &identity,
                                 const std::string &password) {
        nlohmann::json login = {{"identity", identity}, {"password", password}};
        auto res = client.Post("/api/v1/auth/" + entity + "/login",
                               login.dump(), "application/json");
        if (!res || res->status != 200) {
            return "";
        }
        auto body = nlohmann::json::parse(res->body);
        if (!body.contains("data") || !body["data"].contains("token")) {
            return "";
        }
        return body["data"]["token"].get<std::string>();
    }

    inline bool waitForServer(TestHttp::Client& client, int max_retries = 20, int initial_delay_ms = 50) {
        int delay_ms = initial_delay_ms;
        for (int i = 0; i < max_retries; ++i) {
            if (auto res = client.Get("/api/v1/health"); res && res->status == 200) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms = (std::min)(delay_ms * 2, 500);
        }
        return false;
    }

    inline std::string createTestAdminToken(TestHttp::Client& client, mb::MantisBase& app) {
        try {
            auto admin_entity = app.entity("mb_admins");

            auto existing = admin_entity.queryFromCols(TestConfig::getAdminEmail(), {"id", "email"});
            if (!existing.has_value()) {
                nlohmann::json admin_data = {
                    {"email", TestConfig::getAdminEmail()},
                    {"password", TestConfig::getTestPassword()}
                };
                admin_entity.create(admin_data);
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
        } catch (const std::exception&) {
            try {
                auto s_acc = app.entity("mb_service_acc");
                auto record = s_acc.create({});

                nlohmann::json claims;
                claims["id"] = record["id"];
                claims["entity"] = "mb_service_acc";
                auto token = app.auth().createToken(claims, 30 * 60);

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

    inline nlohmann::json publicAccessRules() {
        return {
            {"list", {{"mode", "public"}}},
            {"get", {{"mode", "public"}}},
            {"add", {{"mode", "public"}}},
            {"update", {{"mode", "public"}}},
            {"delete", {{"mode", "public"}}}
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
            schema["rules"] = access_rules;
        }

        auto res = client.Post("/api/v1/schemas", headers, schema.dump(), "application/json");
        return res && (res->status == 201 || res->status == 409);
    }
}

#endif //MANTISBASE_TEST_HELPERS_H
