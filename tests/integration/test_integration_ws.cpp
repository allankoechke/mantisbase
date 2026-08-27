#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <drogon/WebSocketClient.h>
#include <trantor/net/EventLoopThread.h>
#include "../common/test_fixture.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

class IntegrationWSTest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
        port = getPort();
        adminToken = TestHelpers::createTestAdminToken(*client, mantis());
        createTestEntity();
        createAuthOnlyEntity();
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "ws_test_items", adminToken);
        TestHelpers::cleanupTestEntity(*client, "ws_auth_items", adminToken);
        MbServerFixture::TearDown();
    }

    void createTestEntity() {
        if (adminToken.empty()) return;

        const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        const nlohmann::json schema = {
            {"name", "ws_test_items"},
            {"type", "base"},
            {"rules", TestHelpers::publicAccessRules()},
            {
                "fields", nlohmann::json::array({
                    {{"name", "title"}, {"type", "string"}, {"required", true}}
                })
            }
        };

        client->Post("/api/v1/schemas", headers, schema.dump(), "application/json");
    }

    void createAuthOnlyEntity() {
        if (adminToken.empty()) return;

        const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};
        const nlohmann::json rules = {
            {"list", {{"mode", "auth"}}},
            {"get", {{"mode", "auth"}}},
            {"add", {{"mode", "auth"}}},
            {"update", {{"mode", "auth"}}},
            {"delete", {{"mode", "auth"}}}
        };

        const nlohmann::json schema = {
            {"name", "ws_auth_items"},
            {"type", "base"},
            {"rules", rules},
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

struct ScopedWebSocketClient {
    trantor::EventLoopThread loopThread;
    drogon::WebSocketClientPtr client;

    explicit ScopedWebSocketClient(const std::string& url) {
        loopThread.run();
        client = drogon::WebSocketClient::newWebSocketClient(url, loopThread.getLoop());
    }

    void shutdown() {
        if (!client) {
            return;
        }

        if (auto* loop = loopThread.getLoop()) {
            std::promise<void> done;
            auto doneFuture = done.get_future();
            loop->runInLoop([this, &done]() {
                client->setMessageHandler(
                    [](std::string&&, const drogon::WebSocketClientPtr&,
                       const drogon::WebSocketMessageType&) {});
                client->setConnectionClosedHandler(
                    [](const drogon::WebSocketClientPtr&) {});
                client->stop();
                client.reset();
                done.set_value();
            });
            doneFuture.wait();
        } else {
            client->stop();
            client.reset();
        }
    }

    ~ScopedWebSocketClient() { shutdown(); }
};

TEST_F(IntegrationWSTest, WebSocketGuestConnectReceivesClientId) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> welcomePromise;
    auto welcomeFuture = welcomePromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime/ws");

    wsClient->connectToServer(
        req,
        [&welcomePromise, &promiseSet](drogon::ReqResult result,
                              const drogon::HttpResponsePtr &,
                              const drogon::WebSocketClientPtr &wsPtr) {
            ASSERT_EQ(result, drogon::ReqResult::Ok);

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

    ASSERT_EQ(welcomeFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    const auto parsed = nlohmann::json::parse(welcomeFuture.get());
    EXPECT_EQ(parsed["type"], "connected");
    EXPECT_TRUE(parsed["client_id"].get<std::string>().starts_with("rt_ws_"));
    EXPECT_TRUE(parsed["topics"].is_array());
}

TEST_F(IntegrationWSTest, WebSocketInvalidTokenConnectsAsGuest) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> welcomePromise;
    auto welcomeFuture = welcomePromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime/ws?token=invalid.token.here");

    wsClient->connectToServer(
        req,
        [&welcomePromise, &promiseSet](drogon::ReqResult result,
                              const drogon::HttpResponsePtr &,
                              const drogon::WebSocketClientPtr &wsPtr) {
            ASSERT_EQ(result, drogon::ReqResult::Ok);
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

    ASSERT_EQ(welcomeFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    const auto parsed = nlohmann::json::parse(welcomeFuture.get());
    EXPECT_EQ(parsed["type"], "connected");
}

TEST_F(IntegrationWSTest, WebSocketGuestSubscribePublicTopic) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> subPromise;
    auto subFuture = subPromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime/ws");

    wsClient->setMessageHandler(
        [&subPromise, &promiseSet, wsClient](
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
                wsClient->getConnection()->send(sub.dump());
            } else if (parsed["type"] == "subscribed" && !promiseSet) {
                promiseSet = true;
                subPromise.set_value(msg);
            }
        });

    wsClient->connectToServer(
        req,
        [](drogon::ReqResult result,
           const drogon::HttpResponsePtr &,
           const drogon::WebSocketClientPtr &) {
            ASSERT_EQ(result, drogon::ReqResult::Ok);
        });

    ASSERT_EQ(subFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    const auto parsed = nlohmann::json::parse(subFuture.get());
    EXPECT_EQ(parsed["type"], "subscribed");
    ASSERT_TRUE(parsed["topics"].is_array());
    EXPECT_EQ(parsed["topics"].size(), 1u);
    EXPECT_TRUE(parsed["denied"].is_array());
}

TEST_F(IntegrationWSTest, WebSocketGuestSubscribeAuthTopicDenied) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> subPromise;
    auto subFuture = subPromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime/ws");

    wsClient->setMessageHandler(
        [&subPromise, &promiseSet, wsClient](
            const std::string &msg,
            const drogon::WebSocketClientPtr &,
            const drogon::WebSocketMessageType &type) {
            if (type != drogon::WebSocketMessageType::Text)
                return;

            auto parsed = nlohmann::json::parse(msg);
            if (parsed["type"] == "connected") {
                nlohmann::json sub = {
                    {"type", "subscribe"},
                    {"topics", {"ws_auth_items"}}
                };
                wsClient->getConnection()->send(sub.dump());
            } else if (parsed["type"] == "subscribed" && !promiseSet) {
                promiseSet = true;
                subPromise.set_value(msg);
            }
        });

    wsClient->connectToServer(
        req,
        [](drogon::ReqResult result,
           const drogon::HttpResponsePtr &,
           const drogon::WebSocketClientPtr &) {
            ASSERT_EQ(result, drogon::ReqResult::Ok);
        });

    ASSERT_EQ(subFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    const auto parsed = nlohmann::json::parse(subFuture.get());
    EXPECT_EQ(parsed["type"], "subscribed");
    EXPECT_TRUE(parsed["topics"].empty());
    ASSERT_TRUE(parsed["denied"].is_array());
    EXPECT_EQ(parsed["denied"].size(), 1u);
}

TEST_F(IntegrationWSTest, WebSocketConnectsAndReceivesWelcome) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> welcomePromise;
    auto welcomeFuture = welcomePromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(std::format("/api/v1/realtime/ws?token={}", adminToken));

    wsClient->connectToServer(
        req,
        [&welcomePromise, &promiseSet](drogon::ReqResult result,
                              const drogon::HttpResponsePtr &,
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

    ASSERT_EQ(welcomeFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    const auto welcomeMsg = welcomeFuture.get();
    ASSERT_FALSE(welcomeMsg.empty());

    const auto parsed = nlohmann::json::parse(welcomeMsg);
    EXPECT_EQ(parsed["type"], "connected");
    EXPECT_TRUE(parsed.contains("client_id"));
}

TEST_F(IntegrationWSTest, WebSocketPingPong) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> pongPromise;
    auto pongFuture = pongPromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(std::format("/api/v1/realtime/ws?token={}", adminToken));

    wsClient->connectToServer(
        req,
        [&pongPromise, &promiseSet](drogon::ReqResult result,
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
                [&pongPromise, &promiseSet, wsPtr](
                    const std::string &msg,
                    const drogon::WebSocketClientPtr &,
                    const drogon::WebSocketMessageType &type) {
                    if (type != drogon::WebSocketMessageType::Text)
                        return;

                    auto parsed = nlohmann::json::parse(msg);
                    if (parsed["type"] == "connected") {
                        nlohmann::json ping = {{"type", "ping"}};
                        wsPtr->getConnection()->send(ping.dump());
                    } else if (parsed["type"] == "pong" && !promiseSet) {
                        promiseSet = true;
                        pongPromise.set_value(msg);
                    }
                });
        });

    ASSERT_EQ(pongFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_FALSE(pongFuture.get().empty());
}

TEST_F(IntegrationWSTest, WebSocketSubscribeAck) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> subPromise;
    auto subFuture = subPromise.get_future();
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(std::format("/api/v1/realtime/ws?token={}", adminToken));

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

    ASSERT_EQ(subFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    const auto parsed = nlohmann::json::parse(subFuture.get());
    EXPECT_EQ(parsed["type"], "subscribed");
    EXPECT_TRUE(parsed["topics"].is_array());
}
