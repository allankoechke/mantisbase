#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "../common/test_fixture.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

class IntegrationCRUDTest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        createTestEntity();
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "test_products", adminToken);
        TestHelpers::cleanupTestEntity(*client, "test_users", adminToken);
        MbServerFixture::TearDown();
    }

    void createTestEntity() {
        if (adminToken.empty()) return;

        const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        const nlohmann::json schema = {
            {"name", "test_products"},
            {"type", "base"},
            {
                "rules", {
                    {"list", {{"mode", "public"}}},
                    {"get", {{"mode", "public"}}},
                    {"add", {{"mode", "auth"}}},
                    {"update", {{"mode", "auth"}}},
                    {"delete", {{"mode", ""}}}
                }
            },
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

        TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        nlohmann::json userSchema = {
            {"name", "test_users"},
            {"type", "auth"},
            {
                "rules", {
                    {"list", {{"mode", "auth"}}},
                    {"get", {{"mode", "auth"}}},
                    {"add", {{"mode", "public"}}},
                    {"update", {{"mode", "auth"}}},
                    {"delete", {{"mode", ""}}}
                }
            },
            {
                "fields", nlohmann::json::array({
                    {{"name", "name"}, {"type", "string"}, {"required", true}},
                    {{"name", "email"}, {"type", "string"}, {"required", true}, {"unique", true}},
                    {{"name", "password"}, {"type", "string"}, {"required", true}}
                })
            }
        };

        client->Post("/api/v1/schemas", headers, userSchema.dump(), "application/json");

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

        nlohmann::json login = {
            {"identity", "user@test.com"},
            {"password", TestConfig::getTestPassword()}
        };

        auto loginRes = client->Post("/api/v1/auth/test_users/login",
                                     login.dump(), "application/json");

        if (loginRes && loginRes->status == 200) {
            auto response = nlohmann::json::parse(loginRes->body);
            if (response.contains("data") && response["data"].contains("token")) {
                return response["data"]["token"].get<std::string>();
            }
        }

        return "";
    }

    std::unique_ptr<TestHttp::Client> client;
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
    nlohmann::json record = {
        {"name", "List Test Product"},
        {"price", 19.99}
    };
    client->Post("/api/v1/entities/test_products",
                 record.dump(), "application/json");

    auto res = client->Get("/api/v1/entities/test_products", {{"Authorization", "Bearer " + adminToken}});

    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("data") && response["data"].contains("items"));
    EXPECT_TRUE(response["data"]["items"].is_array());
    EXPECT_EQ(response["data"]["items"].size(), response["data"]["items_count"]);
}

TEST_F(IntegrationCRUDTest, GetRecord) {
    nlohmann::json record = {
        {"name", "Get Test Product"},
        {"price", 39.99}
    };

    const TestHttp::Headers headers{{"Authorization", "Bearer " + adminToken}};

    auto createRes = client->Post("/api/v1/entities/test_products",
                                  headers,
                                  record.dump(), "application/json");

    ASSERT_TRUE(createRes != nullptr);
    ASSERT_TRUE(createRes->status == 201);
    ASSERT_FALSE(createRes->body.empty());

    auto createResponse = nlohmann::json::parse(createRes->body);
    std::string recordId = createResponse["data"]["id"];

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

    TestHttp::Headers headers = {{"Authorization", "Bearer " + userToken}};

    nlohmann::json record = {
        {"name", "Update Test Product"},
        {"price", 49.99}
    };

    auto createRes = client->Post("/api/v1/entities/test_products",
                                  headers, record.dump(), "application/json");

    ASSERT_TRUE(createRes != nullptr);
    auto createResponse = nlohmann::json::parse(createRes->body);
    std::string recordId = createResponse["data"]["id"];

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

    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
    auto deleteRes = client->Delete("/api/v1/entities/test_products/" + recordId, headers);

    ASSERT_TRUE(deleteRes != nullptr);
    EXPECT_EQ(deleteRes->status, 204);

    auto getRes = client->Get("/api/v1/entities/test_products/" + recordId, headers);
    EXPECT_EQ(getRes->status, 404);
}

class IntegrationValidationTest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        createSchemas();
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "val_products", adminToken);
        TestHelpers::cleanupTestEntity(*client, "val_users", adminToken);
        TestHelpers::cleanupTestEntity(*client, "val_orders", adminToken);
        MbServerFixture::TearDown();
    }

    void createSchemas() {
        if (adminToken.empty()) return;

        const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        nlohmann::json products = {
            {"name", "val_products"},
            {"type", "base"},
            {"rules", TestHelpers::publicAccessRules()},
            {"fields", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}, {"required", true}},
                {{"name", "price"}, {"type", "double"}, {"required", true}}
            })}
        };
        client->Post("/api/v1/schemas", headers, products.dump(), "application/json");

        nlohmann::json users = {
            {"name", "val_users"},
            {"type", "auth"},
            {"rules", TestHelpers::publicAccessRules()},
            {"fields", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}, {"required", true}},
                {{"name", "email"}, {"type", "string"}, {"required", true},
                 {"constraints", {{"validator", "@email"}}}},
                {{"name", "password"}, {"type", "string"}, {"required", true},
                 {"constraints", {{"validator", "@password"}}}}
            })}
        };
        client->Post("/api/v1/schemas", headers, users.dump(), "application/json");

        nlohmann::json orders = {
            {"name", "val_orders"},
            {"type", "base"},
            {"rules", TestHelpers::publicAccessRules()},
            {"fields", nlohmann::json::array({
                {{"name", "label"}, {"type", "string"}, {"required", true}},
                {{"name", "user_id"}, {"type", "string"},
                 {"foreign_key", {{"entity", "missing_users"}, {"field", "id"}}}}
            })}
        };
        client->Post("/api/v1/schemas", headers, orders.dump(), "application/json");
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
};

TEST_F(IntegrationValidationTest, RejectsMissingRequiredField) {
    auto res = client->Post("/api/v1/entities/val_products",
                            R"({"price": 9.99})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(IntegrationValidationTest, RejectsInvalidEmailPreset) {
    nlohmann::json body = {
        {"name", "Bad User"},
        {"email", "not-an-email"},
        {"password", TestConfig::getTestPassword()}
    };
    auto res = client->Post("/api/v1/entities/val_users", body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(IntegrationValidationTest, RejectsShortPasswordPreset) {
    nlohmann::json body = {
        {"name", "Bad User"},
        {"email", "valid@example.com"},
        {"password", "short"}
    };
    auto res = client->Post("/api/v1/entities/val_users", body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(IntegrationValidationTest, RejectsUnknownFieldOnUpdate) {
    nlohmann::json createBody = {{"name", "Widget"}, {"price", 1.0}};
    auto createRes = client->Post("/api/v1/entities/val_products",
                                  createBody.dump(), "application/json");
    ASSERT_TRUE(createRes);
    ASSERT_EQ(createRes->status, 201);

    auto createJson = nlohmann::json::parse(createRes->body);
    const auto id = createJson["data"]["id"].get<std::string>();

    auto updateRes = client->Patch("/api/v1/entities/val_products/" + id,
                                   TestHttp::Headers{},
                                   R"({"unknown_field":"x"})", "application/json");
    ASSERT_TRUE(updateRes);
    EXPECT_TRUE(updateRes->status == 400 || updateRes->status == 500);
}

TEST_F(IntegrationValidationTest, RejectsForeignKeyToMissingEntity) {
    nlohmann::json body = {
        {"label", "order-1"},
        {"user_id", "00000000-0000-0000-0000-000000000001"}
    };
    auto res = client->Post("/api/v1/entities/val_orders", body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(IntegrationCRUDTest, ListPaginationHasMoreAndCursor) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    for (int i = 0; i < 3; ++i) {
        nlohmann::json product = {
            {"name", "Product " + std::to_string(i)},
            {"price", 1.0 + i},
            {"description", "pagination test"}
        };
        auto createRes = client->Post("/api/v1/entities/test_products", headers,
                                      product.dump(), "application/json");
        ASSERT_TRUE(createRes != nullptr);
        ASSERT_EQ(createRes->status, 201);
    }

    auto page1 = client->Get("/api/v1/entities/test_products?limit=2", headers);
    ASSERT_TRUE(page1 != nullptr);
    EXPECT_EQ(page1->status, 200);

    auto body1 = nlohmann::json::parse(page1->body);
    EXPECT_TRUE(body1["data"]["has_more"].get<bool>());
    EXPECT_EQ(body1["data"]["items_count"].get<int>(), 2);
    EXPECT_FALSE(body1["data"]["cursor"].get<std::string>().empty());

    const auto cursor = body1["data"]["cursor"].get<std::string>();
    auto page2 = client->Get("/api/v1/entities/test_products?limit=2&after=" + cursor, headers);
    ASSERT_TRUE(page2 != nullptr);
    EXPECT_EQ(page2->status, 200);

    auto body2 = nlohmann::json::parse(page2->body);
    EXPECT_FALSE(body2["data"]["has_more"].get<bool>());
    EXPECT_GE(body2["data"]["items_count"].get<int>(), 1);
}

TEST_F(IntegrationCRUDTest, ListFilterByField) {
    TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    client->Post("/api/v1/entities/test_products", headers,
                 nlohmann::json({{"name", "Alpha"}, {"price", 1.0}}).dump(), "application/json");
    client->Post("/api/v1/entities/test_products", headers,
                 nlohmann::json({{"name", "Beta"}, {"price", 2.0}}).dump(), "application/json");

    auto res = client->Get(R"(/api/v1/entities/test_products?filter={"name":"Alpha"})", headers);
    ASSERT_TRUE(res != nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["data"]["items_count"].get<int>(), 1);
    EXPECT_EQ(body["data"]["items"][0]["name"].get<std::string>(), "Alpha");
}
