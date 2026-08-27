#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <trantor/net/EventLoopThread.h>
#include <trantor/net/TcpClient.h>
#include <trantor/net/InetAddress.h>
#include <future>
#include <format>
#include "../common/test_fixture.h"
#include "../common/test_helpers.h"
#include "../common/test_config.h"
#include "../common/test_http_client.h"

namespace {

std::optional<nlohmann::json> parseSseConnectedEvent(const std::string &buffer) {
    const auto eventPos = buffer.find("event: connected");
    if (eventPos == std::string::npos) {
        return std::nullopt;
    }

    const auto dataPos = buffer.find("data: ", eventPos);
    if (dataPos == std::string::npos) {
        return std::nullopt;
    }

    const auto lineEnd = buffer.find('\n', dataPos);
    if (lineEnd == std::string::npos) {
        return std::nullopt;
    }

    std::string jsonStr = buffer.substr(dataPos + 6, lineEnd - dataPos - 6);
    if (!jsonStr.empty() && jsonStr.back() == '\r') {
        jsonStr.pop_back();
    }

    try {
        return nlohmann::json::parse(jsonStr);
    } catch (const nlohmann::json::exception &) {
        return std::nullopt;
    }
}

class ScopedSseConnection {
public:
    ScopedSseConnection(int port, std::string path)
        : port_(port), path_(std::move(path)) {
        loopThread_.run();
        auto *loop = loopThread_.getLoop();

        trantor::InetAddress addr("127.0.0.1", static_cast<uint16_t>(port_));
        tcpClient_ = std::make_shared<trantor::TcpClient>(loop, addr, "sseIntegrationTest");

        tcpClient_->setConnectionCallback([this](const trantor::TcpConnectionPtr &conn) {
            if (conn->connected()) {
                const auto request = std::format(
                    "GET {} HTTP/1.1\r\n"
                    "Host: 127.0.0.1:{}\r\n"
                    "Accept: text/event-stream\r\n"
                    "Connection: keep-alive\r\n\r\n",
                    path_, port_);
                conn->send(request);
            } else {
                connection_.reset();
            }
        });

        tcpClient_->setMessageCallback([this](const trantor::TcpConnectionPtr &conn,
                                              trantor::MsgBuffer *buf) {
            buffer_.append(buf->peek(), buf->readableBytes());
            buf->retrieveAll();

            if (connected_.has_value()) {
                return;
            }

            if (buffer_.find("\r\n\r\n") == std::string::npos) {
                return;
            }

            if (auto parsed = parseSseConnectedEvent(buffer_); parsed.has_value() && parsed->contains("client_id")) {
                connected_ = std::move(*parsed);
                client_id_ = (*connected_)["client_id"].get<std::string>();
                readyPromise_.set_value();
            }

            if (buffer_.size() > 65536) {
                conn->shutdown();
            }
        });

        tcpClient_->connect();
        readyFuture_ = readyPromise_.get_future();
    }

    bool waitForConnected(std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        return readyFuture_.wait_for(timeout) == std::future_status::ready;
    }

    [[nodiscard]] const std::string &clientId() const { return client_id_; }

    [[nodiscard]] const nlohmann::json &connectedPayload() const {
        if (connected_.has_value()) {
            return *connected_;
        }
        return empty_connected_;
    }

    void close() {
        if (closed_) {
            return;
        }
        closed_ = true;

        if (auto *loop = loopThread_.getLoop()) {
            std::promise<void> done;
            auto doneFuture = done.get_future();
            loop->runInLoop([this, &done]() {
                if (tcpClient_) {
                    tcpClient_->disconnect();
                    tcpClient_.reset();
                }
                done.set_value();
            });
            doneFuture.wait_for(std::chrono::seconds(2));
        }
    }

    ~ScopedSseConnection() { close(); }

private:
    int port_{0};
    std::string path_;
    trantor::EventLoopThread loopThread_;
    std::shared_ptr<trantor::TcpClient> tcpClient_;
    trantor::TcpConnectionPtr connection_;
    std::string buffer_;
    std::string client_id_;
    std::optional<nlohmann::json> connected_;
    nlohmann::json empty_connected_{nlohmann::json::object()};
    std::promise<void> readyPromise_;
    std::future<void> readyFuture_;
    bool closed_{false};
};

nlohmann::json parseApiData(const std::string &body) {
    const auto envelope = nlohmann::json::parse(body);
    return envelope.value("data", nlohmann::json::object());
}

} // namespace

class IntegrationSSETest : public MbServerFixture {
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
        TestHelpers::cleanupTestEntity(*client, "sse_test_items", adminToken);
        TestHelpers::cleanupTestEntity(*client, "sse_auth_items", adminToken);
        MbServerFixture::TearDown();
    }

    void createTestEntity() {
        if (adminToken.empty()) return;

        const TestHttp::Headers headers = {{"Authorization", "Bearer " + adminToken}};

        const nlohmann::json schema = {
            {"name", "sse_test_items"},
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
            {"name", "sse_auth_items"},
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

TEST_F(IntegrationSSETest, SSEEndpointRejects400OnInvalidTopic) {
    auto res = client->Get("/api/v1/realtime?topics=nonexistent_entity");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    EXPECT_NE(res->status, 503);
}

TEST_F(IntegrationSSETest, SSEGuestConnectReceivesRtSseClientId) {
    ScopedSseConnection sse(port, "/api/v1/realtime");
    ASSERT_TRUE(sse.waitForConnected());
    EXPECT_TRUE(sse.clientId().starts_with("rt_sse_"));
    EXPECT_TRUE(sse.connectedPayload()["topics"].is_array());
    EXPECT_TRUE(sse.connectedPayload()["topics"].empty());
}

TEST_F(IntegrationSSETest, SSEConnectWithPublicTopicsIncludesGrantedTopics) {
    ScopedSseConnection sse(port, "/api/v1/realtime?topics=sse_test_items");
    ASSERT_TRUE(sse.waitForConnected());
    EXPECT_TRUE(sse.clientId().starts_with("rt_sse_"));

    const auto &topics = sse.connectedPayload()["topics"];
    ASSERT_TRUE(topics.is_array());
    ASSERT_EQ(topics.size(), 1);
    EXPECT_EQ(topics[0].get<std::string>(), "sse_test_items");
}

TEST_F(IntegrationSSETest, SSEPostUpdateReturns404ForUnknownSession) {
    nlohmann::json body = {
        {"client_id", "nonexistent_session_id"},
        {"topics", nlohmann::json::array({"sse_test_items"})}
    };

    auto res = client->Post("/api/v1/realtime",
                            TestHttp::Headers{{"Authorization", "Bearer " + adminToken}},
                            body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(IntegrationSSETest, SSEPostSubscribeUpgradesGuestAuth) {
    ScopedSseConnection sse(port, "/api/v1/realtime");
    ASSERT_TRUE(sse.waitForConnected());
    ASSERT_FALSE(sse.clientId().empty());

    nlohmann::json body = {
        {"client_id", sse.clientId()},
        {"topics", nlohmann::json::array({"sse_test_items"})}
    };

    auto res = client->Post("/api/v1/realtime",
                            TestHttp::Headers{{"Authorization", "Bearer " + adminToken}},
                            body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    const auto data = parseApiData(res->body);
    EXPECT_EQ(data["client_id"].get<std::string>(), sse.clientId());
    ASSERT_TRUE(data["topics"].is_array());
    ASSERT_EQ(data["topics"].size(), 1);
    EXPECT_EQ(data["topics"][0].get<std::string>(), "sse_test_items");
}

TEST_F(IntegrationSSETest, SSEPostMixedTopicsReturnsDeniedForGuest) {
    ScopedSseConnection sse(port, "/api/v1/realtime");
    ASSERT_TRUE(sse.waitForConnected());

    nlohmann::json body = {
        {"client_id", sse.clientId()},
        {"topics", nlohmann::json::array({"sse_test_items", "sse_auth_items"})}
    };

    auto res = client->Post("/api/v1/realtime", TestHttp::Headers{}, body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);

    const auto data = parseApiData(res->body);
    ASSERT_TRUE(data["topics"].is_array());
    ASSERT_EQ(data["topics"].size(), 1);
    EXPECT_EQ(data["topics"][0].get<std::string>(), "sse_test_items");

    ASSERT_TRUE(data["denied"].is_array());
    ASSERT_EQ(data["denied"].size(), 1);
    EXPECT_EQ(data["denied"][0]["topic"].get<std::string>(), "sse_auth_items");
}

TEST_F(IntegrationSSETest, SSEPostAuthSwitchReturns403) {
    ScopedSseConnection sse(port, std::format("/api/v1/realtime?token={}", adminToken));
    ASSERT_TRUE(sse.waitForConnected());

    nlohmann::json claims;
    claims["id"] = "other-admin-id";
    claims["entity"] = "mb_admins";
    const auto otherToken = mantis().auth().createToken(claims, 30 * 60);

    nlohmann::json body = {
        {"client_id", sse.clientId()},
        {"topics", nlohmann::json::array({"sse_test_items"})},
        {"token", otherToken}
    };

    auto res = client->Post("/api/v1/realtime", TestHttp::Headers{}, body.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 403);
}
