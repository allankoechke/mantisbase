#include <gtest/gtest.h>
#include <drogon/HttpRequest.h>
#include <trantor/net/InetAddress.h>
#include <HttpRequestImpl.h>
#include <optional>
#include <cstdlib>

#include "mantisbase/core/http.h"
#include "../common/test_fixture.h"
#include "../common/test_helpers.h"

namespace {

class ScopedEnvVar {
public:
    explicit ScopedEnvVar(const char *key, std::optional<std::string> value)
        : key_(key) {
        if (const char *prev = std::getenv(key)) {
            previous_ = prev;
        }
        TestHelpers::setEnvVar(key, value);
    }

    ~ScopedEnvVar() {
        TestHelpers::setEnvVar(key_, previous_);
    }

private:
    const char *key_;
    std::optional<std::string> previous_;
};

void setRequestPeer(const drogon::HttpRequestPtr &req, const std::string &ip, uint16_t port = 12345) {
    auto impl = std::dynamic_pointer_cast<drogon::HttpRequestImpl>(req);
    ASSERT_NE(impl, nullptr);
    impl->setPeerAddr(trantor::InetAddress(ip, port));
}

drogon::HttpRequestPtr makeDrogonRequest(const std::string &peer_ip = "127.0.0.1") {
    auto req = drogon::HttpRequest::newHttpRequest();
    setRequestPeer(req, peer_ip);
    return req;
}

} // namespace

class HttpRequestTest : public MbAppFixture {
protected:
    mb::MantisRequest makeRequest(const std::string &peer_ip = "127.0.0.1") {
        return mb::MantisRequest(mantis(), makeDrogonRequest(peer_ip));
    }
};

TEST_F(HttpRequestTest, AttributeStorage) {
    auto req = makeRequest();

    EXPECT_FALSE(req.hasKey("user_id"));
    req.set<std::string>("user_id", "abc-123");
    EXPECT_TRUE(req.hasKey("user_id"));
}

TEST_F(HttpRequestTest, BearerTokenAuth) {
    auto req = makeRequest();
    EXPECT_TRUE(req.getBearerTokenAuth().empty());

    auto drogonReq = req.drogonRequest();
    drogonReq->addHeader("Authorization", "Bearer test-token-123");
    mb::MantisRequest req2(mantis(), drogonReq);
    EXPECT_EQ(req2.getBearerTokenAuth(), "test-token-123");

    drogonReq->removeHeader("Authorization");
    drogonReq->addHeader("Authorization", "Bearer");
    mb::MantisRequest req3(mantis(), drogonReq);
    EXPECT_TRUE(req3.getBearerTokenAuth().empty());
}

TEST_F(HttpRequestTest, GetBodyAsJson) {
    auto req = makeRequest();
    auto [emptyBody, emptyErr] = req.getBodyAsJson();
    EXPECT_TRUE(emptyErr.empty());
    EXPECT_TRUE(emptyBody.is_object());

    auto drogonReq = req.drogonRequest();
    drogonReq->setBody(R"({"name":"test"})");
    mb::MantisRequest req2(mantis(), drogonReq);
    auto [body, err] = req2.getBodyAsJson();
    EXPECT_TRUE(err.empty());
    EXPECT_EQ(body["name"], "test");

    drogonReq->setBody("{invalid json");
    mb::MantisRequest req3(mantis(), drogonReq);
    auto [badBody, badErr] = req3.getBodyAsJson();
    EXPECT_FALSE(badErr.empty());
    EXPECT_TRUE(badBody.is_object());
}

TEST_F(HttpRequestTest, GetRemoteAddrWithoutForwardedHeader) {
    const mb::MantisRequest req = makeRequest("192.0.2.10");
    EXPECT_EQ(req.getRemoteAddr(), "192.0.2.10");
}

TEST_F(HttpRequestTest, GetRemoteAddrIgnoresForwardedHeaderWhenTrustedProxiesUnset) {
    ScopedEnvVar clear_trusted("MB_TRUSTED_PROXIES", std::nullopt);

    auto drogonReq = makeDrogonRequest("192.0.2.20");
    drogonReq->addHeader("X-Forwarded-For", "203.0.113.1, 198.51.100.2");
    const mb::MantisRequest req(mantis(), drogonReq);

    EXPECT_EQ(req.getRemoteAddr(), "192.0.2.20");
}

TEST_F(HttpRequestTest, GetRemoteAddrIgnoresForwardedHeaderWhenPeerNotTrusted) {
    ScopedEnvVar trusted("MB_TRUSTED_PROXIES", "10.0.0.1");

    auto drogonReq = makeDrogonRequest("192.0.2.30");
    drogonReq->addHeader("X-Forwarded-For", "203.0.113.1");
    const mb::MantisRequest req(mantis(), drogonReq);

    EXPECT_EQ(req.getRemoteAddr(), "192.0.2.30");
}

TEST_F(HttpRequestTest, GetRemoteAddrUsesForwardedHeaderFromTrustedProxy) {
    ScopedEnvVar trusted("MB_TRUSTED_PROXIES", "10.0.0.1");

    auto drogonReq = makeDrogonRequest("10.0.0.1");
    drogonReq->addHeader("X-Forwarded-For", "203.0.113.1, 198.51.100.2");
    const mb::MantisRequest req(mantis(), drogonReq);

    EXPECT_EQ(req.getRemoteAddr(), "203.0.113.1");
}

TEST_F(HttpRequestTest, GetRemoteAddrMatchesTrustedProxyFromCommaSeparatedList) {
    ScopedEnvVar trusted("MB_TRUSTED_PROXIES", "10.0.0.1, 10.0.0.2");

    auto drogonReq = makeDrogonRequest("10.0.0.2");
    drogonReq->addHeader("X-Forwarded-For", "203.0.113.5");
    const mb::MantisRequest req(mantis(), drogonReq);

    EXPECT_EQ(req.getRemoteAddr(), "203.0.113.5");
}

TEST_F(HttpRequestTest, GetRemoteAddrIgnoresInvalidForwardedHeaderFromTrustedProxy) {
    ScopedEnvVar trusted("MB_TRUSTED_PROXIES", "10.0.0.1");

    auto drogonReq = makeDrogonRequest("10.0.0.1");
    drogonReq->addHeader("X-Forwarded-For", "not-an-ip");
    const mb::MantisRequest req(mantis(), drogonReq);

    EXPECT_EQ(req.getRemoteAddr(), "10.0.0.1");
}
