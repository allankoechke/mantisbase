#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "../common/test_environment.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

class MultiInstanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& fixture = MbTestEnv::instance();
        client = std::make_unique<TestHttp::Client>(fixture.client());
        port = fixture.getPort();

        adminToken = TestHelpers::createTestAdminToken(*client);
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "multi_inst_a", adminToken);
        TestHelpers::cleanupTestEntity(*client, "multi_inst_b", adminToken);
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
    int port{0};
};

TEST_F(MultiInstanceTest, ServerServesOnConfiguredPort) {
    auto res = client->Get("/api/v1/health");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
}

TEST_F(MultiInstanceTest, IndependentEntitiesDontInterfere) {
    ASSERT_FALSE(adminToken.empty());

    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema_a = {
        {"name", "multi_inst_a"},
        {"type", "base"},
        {"list", {{"mode", "public"}}},
        {"get", {{"mode", "public"}}},
        {"add", {{"mode", "public"}}},
        {"update", {{"mode", "public"}}},
        {"delete", {{"mode", "public"}}},
        {"fields", nlohmann::json::array({
            {{"name", "label"}, {"type", "string"}, {"required", true}}
        })}
    };

    const nlohmann::json schema_b = {
        {"name", "multi_inst_b"},
        {"type", "base"},
        {"list", {{"mode", "public"}}},
        {"get", {{"mode", "public"}}},
        {"add", {{"mode", "public"}}},
        {"update", {{"mode", "public"}}},
        {"delete", {{"mode", "public"}}},
        {"fields", nlohmann::json::array({
            {{"name", "tag"}, {"type", "string"}, {"required", true}}
        })}
    };

    auto res_a = client->Post("/api/v1/schemas", headers, schema_a.dump(), "application/json");
    ASSERT_TRUE(res_a);
    EXPECT_EQ(res_a->status, 201);

    auto res_b = client->Post("/api/v1/schemas", headers, schema_b.dump(), "application/json");
    ASSERT_TRUE(res_b);
    EXPECT_EQ(res_b->status, 201);

    nlohmann::json record_a = {{"label", "item_a"}};
    auto insert_a = client->Post("/api/v1/entities/multi_inst_a",
                                  record_a.dump(), "application/json");
    ASSERT_TRUE(insert_a);
    EXPECT_EQ(insert_a->status, 201);

    nlohmann::json record_b = {{"tag", "item_b"}};
    auto insert_b = client->Post("/api/v1/entities/multi_inst_b",
                                  record_b.dump(), "application/json");
    ASSERT_TRUE(insert_b);
    EXPECT_EQ(insert_b->status, 201);

    auto list_a = client->Get("/api/v1/entities/multi_inst_a");
    ASSERT_TRUE(list_a);
    EXPECT_EQ(list_a->status, 200);
    auto body_a = nlohmann::json::parse(list_a->body);
    EXPECT_EQ(body_a["data"]["items_count"], 1);

    auto list_b = client->Get("/api/v1/entities/multi_inst_b");
    ASSERT_TRUE(list_b);
    EXPECT_EQ(list_b->status, 200);
    auto body_b = nlohmann::json::parse(list_b->body);
    EXPECT_EQ(body_b["data"]["items_count"], 1);
}

TEST_F(MultiInstanceTest, ConcurrentRequestsToSameInstance) {
    ASSERT_FALSE(adminToken.empty());

    const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

    const nlohmann::json schema = {
        {"name", "multi_inst_a"},
        {"type", "base"},
        {"list", {{"mode", "public"}}},
        {"get", {{"mode", "public"}}},
        {"add", {{"mode", "public"}}},
        {"fields", nlohmann::json::array({
            {{"name", "label"}, {"type", "string"}, {"required", true}}
        })}
    };

    client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    const int num_requests = 20;

    for (int i = 0; i < num_requests; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            TestHttp::Client thread_client("127.0.0.1", port);
            nlohmann::json record = {{"label", "concurrent_" + std::to_string(i)}};
            auto res = thread_client.Post("/api/v1/entities/multi_inst_a",
                                          record.dump(), "application/json");
            if (res && res->status == 201) {
                success_count.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_requests);

    auto list_res = client->Get("/api/v1/entities/multi_inst_a");
    ASSERT_TRUE(list_res);
    EXPECT_EQ(list_res->status, 200);
    auto body = nlohmann::json::parse(list_res->body);
    EXPECT_EQ(body["data"]["items_count"], num_requests);
}

TEST_F(MultiInstanceTest, SubsystemsAccessible) {
    auto& app = mb::MantisBase::instance();

    EXPECT_NO_THROW(app.db());
    EXPECT_NO_THROW(app.router());
    EXPECT_NO_THROW(app.settings());
    EXPECT_NO_THROW(app.logs());
    EXPECT_NO_THROW(app.rt());

    EXPECT_GT(app.port(), 0);
    EXPECT_FALSE(app.host().empty());
    EXPECT_FALSE(app.dataDir().empty());
    EXPECT_FALSE(app.appVersion().empty());
}
