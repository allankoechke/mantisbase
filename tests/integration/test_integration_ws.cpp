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
    }

    void TearDown() override {
        TestHelpers::cleanupTestEntity(*client, "ws_test_items", adminToken);
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

    std::unique_ptr<TestHttp::Client> client;
    std::string adminToken;
    int port{0};

    void expectUnauthorizedWsConnection(drogon::WebSocketClientPtr& wsClient,
                                        const std::string& wsPath) {
        std::promise<void> donePromise;
        auto doneFuture = donePromise.get_future();
        bool welcomeReceived = false;
        bool closeVerified = false;
        bool done = false;

        auto req = drogon::HttpRequest::newHttpRequest();
        req->setPath(wsPath);

        wsClient->connectToServer(
            req,
            [&](drogon::ReqResult result,
                const drogon::HttpResponsePtr&,
                const drogon::WebSocketClientPtr& wsPtr) {
                ASSERT_EQ(result, drogon::ReqResult::Ok);

                wsPtr->setConnectionClosedHandler(
                    [wsPtr, &done, &donePromise](const drogon::WebSocketClientPtr&) {
                        wsPtr->stop();
                        if (!done) {
                            done = true;
                            donePromise.set_value();
                        }
                    });

                wsPtr->setMessageHandler(
                    [&welcomeReceived, &closeVerified, &done, &donePromise](
                        const std::string& msg,
                        const drogon::WebSocketClientPtr& wsPtr,
                        const drogon::WebSocketMessageType& type) {
                        if (type == drogon::WebSocketMessageType::Text) {
                            welcomeReceived = true;
                            return;
                        }
                        if (type != drogon::WebSocketMessageType::Close || done) {
                            return;
                        }

                        ASSERT_GE(msg.size(), 2u);
                        const uint16_t closeCode =
                            (static_cast<uint8_t>(msg[0]) << 8) |
                            static_cast<uint8_t>(msg[1]);
                        EXPECT_EQ(closeCode,
                                  static_cast<uint16_t>(drogon::CloseCode::kViolation));
                        if (msg.size() > 2) {
                            EXPECT_EQ(msg.substr(2), "Unauthorized");
                        }

                        closeVerified = true;
                        done = true;
                        wsPtr->stop();
                        donePromise.set_value();
                    });
            });

        const auto status = doneFuture.wait_for(std::chrono::seconds(5));
        ASSERT_EQ(status, std::future_status::ready);
        EXPECT_FALSE(welcomeReceived);
        EXPECT_TRUE(closeVerified);
    }
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
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    auto& wsClient = ws.client;

    std::promise<std::string> pongPromise;
    auto pongFuture = pongPromise.get_future();
    bool welcomeReceived = false;
    bool promiseSet = false;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(std::format("/api/v1/realtime/ws?token={}", adminToken));

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

    auto status = subFuture.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(status, std::future_status::ready);

    auto subMsg = subFuture.get();
    ASSERT_FALSE(subMsg.empty());

    auto parsed = nlohmann::json::parse(subMsg);
    EXPECT_EQ(parsed["type"], "subscribed");
    EXPECT_TRUE(parsed["topics"].is_array());
}

TEST_F(IntegrationWSTest, WebSocketRejectsMissingToken) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    expectUnauthorizedWsConnection(ws.client, "/api/v1/realtime/ws");
}

TEST_F(IntegrationWSTest, WebSocketRejectsInvalidToken) {
    ScopedWebSocketClient ws(std::format("ws://127.0.0.1:{}", port));
    expectUnauthorizedWsConnection(ws.client,
                                   "/api/v1/realtime/ws?token=invalid.token.here");
}
