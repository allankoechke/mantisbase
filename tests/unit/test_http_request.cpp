#include <gtest/gtest.h>
#include <drogon/HttpRequest.h>
#include "mantisbase/core/http.h"
#include "../common/test_fixture.h"

class HttpRequestTest : public MbAppFixture {
protected:
    mb::MantisRequest makeRequest() {
        return mb::MantisRequest(mantis(), drogon::HttpRequest::newHttpRequest());
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
    auto req = makeRequest();
    EXPECT_NO_THROW(req.getRemoteAddr());
}

TEST_F(HttpRequestTest, GetRemoteAddrWithValidForwardedHeader) {
    auto drogonReq = drogon::HttpRequest::newHttpRequest();
    drogonReq->addHeader("X-Forwarded-For", "203.0.113.1, 198.51.100.2");
    mb::MantisRequest req(mantis(), drogonReq);
    EXPECT_EQ(req.getRemoteAddr(), "203.0.113.1");
}

TEST_F(HttpRequestTest, GetRemoteAddrIgnoresInvalidForwardedHeader) {
    auto drogonReq = drogon::HttpRequest::newHttpRequest();
    drogonReq->addHeader("X-Forwarded-For", "not-an-ip");
    mb::MantisRequest req(mantis(), drogonReq);
    EXPECT_NO_THROW(req.getRemoteAddr());
}
