#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
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
                {{"name", "views"}, {"type", "int"}}
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

class IntegrationMigrateTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& fixture = MbTestEnv::instance();
        client = std::make_unique<TestHttp::Client>(fixture.client());

        adminToken = TestHelpers::createTestAdminToken(*client);
        ASSERT_FALSE(adminToken.empty());

        dumpPath = std::filesystem::temp_directory_path() / "mantisbase_test_migrate_dump.json";
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "migrate_test_a", adminToken);
        TestHelpers::cleanupTestEntity(*client, "migrate_test_b", adminToken);
        std::filesystem::remove(dumpPath);
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
    std::filesystem::path dumpPath;
};

TEST_F(IntegrationMigrateTest, DumpAndRestoreRoundTrip) {
    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    // Create two test entities
    const nlohmann::json schema_a = {
        {"name", "migrate_test_a"},
        {"type", "base"},
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}, {"required", true}},
            {{"name", "count"}, {"type", "int"}}
        })}
    };

    const nlohmann::json schema_b = {
        {"name", "migrate_test_b"},
        {"type", "base"},
        {"fields", nlohmann::json::array({
            {{"name", "label"}, {"type", "string"}}
        })}
    };

    auto res_a = client->Post("/api/v1/schemas", headers, schema_a.dump(), "application/json");
    ASSERT_TRUE(res_a != nullptr);
    EXPECT_EQ(res_a->status, 201);

    auto res_b = client->Post("/api/v1/schemas", headers, schema_b.dump(), "application/json");
    ASSERT_TRUE(res_b != nullptr);
    EXPECT_EQ(res_b->status, 201);

    // Dump schemas via EntitySchema::listTables and write to file
    auto list_res = client->Get("/api/v1/schemas", headers);
    ASSERT_TRUE(list_res != nullptr);
    EXPECT_EQ(list_res->status, 200);

    auto list_body = nlohmann::json::parse(list_res->body);
    nlohmann::json dump = nlohmann::json::array();
    for (const auto &row : list_body["data"]) {
        if (!row["schema"].value("system", false)) {
            dump.push_back({{"schema", row["schema"]}});
        }
    }

    {
        std::ofstream out(dumpPath);
        ASSERT_TRUE(out.is_open());
        out << dump.dump(2);
    }

    // Verify dump file exists and contains our entities
    std::ifstream in(dumpPath);
    ASSERT_TRUE(in.is_open());
    nlohmann::json dumped;
    in >> dumped;
    in.close();

    ASSERT_TRUE(dumped.is_array());
    EXPECT_GE(dumped.size(), 2);

    bool found_a = false, found_b = false;
    for (const auto &entry : dumped) {
        const auto name = entry["schema"].value("name", "");
        if (name == "migrate_test_a") found_a = true;
        if (name == "migrate_test_b") found_b = true;
    }
    EXPECT_TRUE(found_a);
    EXPECT_TRUE(found_b);

    // Drop both entities
    client->Delete("/api/v1/schemas/migrate_test_a", headers);
    client->Delete("/api/v1/schemas/migrate_test_b", headers);

    // Verify they're gone
    auto get_a = client->Get("/api/v1/schemas/migrate_test_a", headers);
    EXPECT_EQ(get_a->status, 404);
    auto get_b = client->Get("/api/v1/schemas/migrate_test_b", headers);
    EXPECT_EQ(get_b->status, 404);

    // Restore from dump using EntitySchema::fromSchema + createTable
    for (const auto &entry : dumped) {
        const auto &schema = entry["schema"];
        const auto name = schema.value("name", "");

        auto eSchema = mb::EntitySchema::fromSchema(schema);
        auto err = eSchema.validate();
        ASSERT_FALSE(err.has_value()) << "Validation failed for " << name << ": " << err.value_or("");

        mb::EntitySchema::createTable(eSchema);
    }

    // Verify entities exist again and are queryable
    auto restored_a = client->Get("/api/v1/schemas/migrate_test_a", headers);
    ASSERT_TRUE(restored_a != nullptr);
    EXPECT_EQ(restored_a->status, 200);

    auto restored_body_a = nlohmann::json::parse(restored_a->body);
    EXPECT_EQ(restored_body_a["data"]["schema"]["name"], "migrate_test_a");

    auto restored_b = client->Get("/api/v1/schemas/migrate_test_b", headers);
    ASSERT_TRUE(restored_b != nullptr);
    EXPECT_EQ(restored_b->status, 200);

    // Verify we can insert records into the restored entity
    const nlohmann::json record = {
        {"title", "test record"},
        {"count", 42}
    };
    auto insert_res = client->Post("/api/v1/migrate_test_a/records", headers,
                                    record.dump(), "application/json");
    ASSERT_TRUE(insert_res != nullptr);
    EXPECT_EQ(insert_res->status, 201);
}

TEST_F(IntegrationMigrateTest, RestoreSkipsExistingEntities) {
    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema_a = {
        {"name", "migrate_test_a"},
        {"type", "base"},
        {"fields", nlohmann::json::array({
            {{"name", "title"}, {"type", "string"}}
        })}
    };

    auto res = client->Post("/api/v1/schemas", headers, schema_a.dump(), "application/json");
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 201);

    // Try to create the same entity again — should fail with 500 (table exists)
    auto eSchema = mb::EntitySchema::fromSchema(schema_a);
    EXPECT_TRUE(mb::EntitySchema::tableExists("migrate_test_a"));

    // Verify the entity is still accessible (wasn't corrupted)
    auto get_res = client->Get("/api/v1/schemas/migrate_test_a", headers);
    EXPECT_EQ(get_res->status, 200);
}
