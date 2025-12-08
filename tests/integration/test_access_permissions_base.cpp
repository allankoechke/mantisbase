//
// Created by allan on 18/06/2025.
//
#include "test_access_permissions_base.h"

void AccessPermissionTest::cleanupTestData() const
{
    // Delete test tables and data created by this test
    // client->Delete("/api/v1/tables/test_permissions");
    // client->Delete("/api/v1/tables/admin_only");
    // etc.
}

void AccessPermissionTest::createTestTableWithRules() const
{
    // Create a table with specific access rules for testing
    const nlohmann::json table_schema = {
        {"name", "test_permissions"},
        {"type", "base"},
        {"listRule", "auth.table == 'users'"},           // Only users can list
        {"getRule", "auth.id == req.id"},                // Users can only get their own records
        {"addRule", "auth.table == 'users'"},            // Only users can add
        {"updateRule", "auth.id == req.id"},             // Users can only update their own records
        {"deleteRule", "auth.table == 'mb_admin'"},       // Only admins can delete
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}, {"required", true}},
            {{"name", "user_id"}, {"type", "string"}, {"required", true}}
        })}
    };

    // Create table through system API
    // auto result = client->Post("/api/v1/tables",
    //     table_schema.dump(), "application/json");
}

[[nodiscard]]
std::string AccessPermissionTest::createUserAndGetToken(const std::string& email) const
{
    // Create user
    const nlohmann::json user = {
        {"name", "Test User"},
        {"email", email},
        {"password", "testpass123"}
    };

    // client->Post("/api/v1/users", user.dump(), "application/json");
    //
    // // Login and get token
    // const nlohmann::json login = {{"email", email}, {"password", "testpass123"}};
    // auto auth_result = client->Post("/api/v1/users/auth-with-password",
    //     login.dump(), "application/json");
    //
    // auto response = nlohmann::json::parse(auth_result->body);
    // return response["data"]["token"].get<std::string>();
    return "";
}

[[nodiscard]]
std::string AccessPermissionTest::createAdminAndGetToken() const
{
    // Create admin user
    const nlohmann::json admin = {
        {"name", "Admin User"},
        {"email", "admin@test.com"},
        {"password", "adminpass123"}
    };

    // client->Post("/api/v1/admins", admin.dump(), "application/json");
    //
    // // Login as admin
    // const nlohmann::json login = {{"email", "admin@test.com"}, {"password", "adminpass123"}};
    // auto auth_result = client->Post("/api/v1/admins/auth-with-password",
    //     login.dump(), "application/json");
    //
    // auto response = nlohmann::json::parse(auth_result->body);
    // return response["data"]["token"].get<std::string>();
    return "";
}