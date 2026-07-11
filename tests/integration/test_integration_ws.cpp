#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <drogon/WebSocketClient.h>
#include <trantor/net/EventLoopThread.h>
#include "../common/test_environment.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

class IntegrationWSTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto &fixture = MbTestEnv::instance();
        client = std::make_unique<TestHttp::Client>(fixture.client());
        port = fixture.getPort();

        adminToken = TestHelpers::createTestAdminToken(*client);
        createTestEntity();
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "ws_test_items", adminToken);
    }

    void createTestEntity() {
        if (adminToken.empty()) return;

        const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        const nlohmann::json schema = {
            {"name", "ws_test_items"},
            {"type", "base"},
            {"list", {{"mode", "public"}}},
            {"get", {{"mode", "public"}}},
            {"add", {{"mode", "public"}}},
            {"update", {{"mode", "public"}}},
            {"delete", {{"mode", "public"}}},
            {
                "fields", nlohmann::json::array({
                    {{"name", "title"}, {"type", "string"}, {"required", true}}
                })
            }
        };

        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    }

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
    int port{0};
};

TEST_F(IntegrationWSTest, WebSocketConnectsAndReceivesWelcome) {
    trantor::EventLoopThread loopThread;
    loopThread.run();

    auto wsClient = drogon::WebSocketClient::newWebSocketClient(
        std::format("ws://127.0.0.1:{}", port), loopThread.getLoop());

    std::promise<std::string> welcomePromise;
    auto welcomeFuture = welcomePromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime/ws");

    wsClient->connectToServer(
        req,
        [&welcomePromise, &promiseSet](drogon::ReqResult result,
                              const drogon::HttpResponsePtr &resp,
                              const drogon::WebSocketClientPtr &wsPtr) {
            if (result != drogon::ReqResult::Ok) {
                if (!promiseSet) {
                    promiseSet = true;
                    welcomePromise.set_value("");
                }
                return;
            }

            wsPtr->setMessageHandler(
                [&welcomePromise, &promiseSet](const std::string &msg,
                                     const drogon::WebSocketClientPtr &,
                                     const drogon::WebSocketMessageType &type) {
                    if (type == drogon::WebSocketMessageType::Text && !promiseSet) {
                        promiseSet = true;
                        welcomePromise.set_value(msg);
                    }
                });
        });

    auto status = welcomeFuture.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);

    auto welcomeMsg = welcomeFuture.get();
    ASSERT_FALSE(welcomeMsg.empty());

    auto parsed = nlohmann::json::parse(welcomeMsg);
    EXPECT_EQ(parsed["type"], "connected");
}

TEST_F(IntegrationWSTest, WebSocketPingPong) {
    trantor::EventLoopThread loopThread;
    loopThread.run();

    auto wsClient = drogon::WebSocketClient::newWebSocketClient(
        std::format("ws://127.0.0.1:{}", port), loopThread.getLoop());

    std::promise<std::string> pongPromise;
    auto pongFuture = pongPromise.get_future();
    bool welcomeReceived = false;
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime/ws");

    wsClient->connectToServer(
        req,
        [&pongPromise, &welcomeReceived, &promiseSet](drogon::ReqResult result,
                              const drogon::HttpResponsePtr &,
                              const drogon::WebSocketClientPtr &wsPtr) {
            if (result != drogon::ReqResult::Ok) {
                if (!promiseSet) {
                    promiseSet = true;
                    pongPromise.set_value("");
                }
                return;
            }

            wsPtr->setMessageHandler(
                [&pongPromise, &welcomeReceived, &promiseSet, wsPtr](
                    const std::string &msg,
                    const drogon::WebSocketClientPtr &,
                    const drogon::WebSocketMessageType &type) {
                    if (type != drogon::WebSocketMessageType::Text)
                        return;

                    auto parsed = nlohmann::json::parse(msg);
                    if (parsed["type"] == "connected") {
                        welcomeReceived = true;
                        nlohmann::json ping = {{"type", "ping"}};
                        wsPtr->getConnection()->send(ping.dump());
                    } else if (parsed["type"] == "pong" && !promiseSet) {
                        promiseSet = true;
                        pongPromise.set_value(msg);
                    }
                });
        });

    auto status = pongFuture.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);

    auto pongMsg = pongFuture.get();
    ASSERT_FALSE(pongMsg.empty());

    auto parsed = nlohmann::json::parse(pongMsg);
    EXPECT_EQ(parsed["type"], "pong");
}

TEST_F(IntegrationWSTest, WebSocketSubscribeAck) {
    trantor::EventLoopThread loopThread;
    loopThread.run();

    auto wsClient = drogon::WebSocketClient::newWebSocketClient(
        std::format("ws://127.0.0.1:{}", port), loopThread.getLoop());

    std::promise<std::string> subPromise;
    auto subFuture = subPromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime/ws");

    wsClient->connectToServer(
        req,
        [&subPromise, &promiseSet](drogon::ReqResult result,
                              const drogon::HttpResponsePtr &,
                              const drogon::WebSocketClientPtr &wsPtr) {
            if (result != drogon::ReqResult::Ok) {
                if (!promiseSet) {
                    promiseSet = true;
                    subPromise.set_value("");
                }
                return;
            }

            wsPtr->setMessageHandler(
                [&subPromise, &promiseSet, wsPtr](
                    const std::string &msg,
                    const drogon::WebSocketClientPtr &,
                    const drogon::WebSocketMessageType &type) {
                    if (type != drogon::WebSocketMessageType::Text)
                        return;

                    auto parsed = nlohmann::json::parse(msg);
                    if (parsed["type"] == "connected") {
                        nlohmann::json sub = {
                            {"type", "subscribe"},
                            {"topics", {"ws_test_items"}}
                        };
                        wsPtr->getConnection()->send(sub.dump());
                    } else if (parsed["type"] == "subscribed" && !promiseSet) {
                        promiseSet = true;
                        subPromise.set_value(msg);
                    }
                });
        });

    auto status = subFuture.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);

    auto subMsg = subFuture.get();
    ASSERT_FALSE(subMsg.empty());

    auto parsed = nlohmann::json::parse(subMsg);
    EXPECT_EQ(parsed["type"], "subscribed");
    EXPECT_TRUE(parsed["topics"].is_array());
}
