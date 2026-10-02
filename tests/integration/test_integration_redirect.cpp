#include <gtest/gtest.h>

#include "../common/test_fixture.h"
#include "../common/test_http_client.h"
#include "mantisbase/core/exceptions.h"

class IntegrationRedirectTest : public MbServerFixture {
protected:
    void SetUp() override {
        MbServerFixture::SetUp();
        client = std::make_unique<TestHttp::Client>("127.0.0.1", getPort());
    }

    std::unique_ptr<TestHttp::Client> client;
};

TEST_F(IntegrationRedirectTest, RedirectTargetIsReachable) {
    const auto res = client->Get("/api/v1/test/redirect/target");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("redirect_target"), std::string::npos);
}

TEST_F(IntegrationRedirectTest, RedirectDefaultsTo302WithLocation) {
    const auto res = client->Get("/api/v1/test/redirect/old");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 302);
    EXPECT_EQ(res->header("Location"), "/api/v1/test/redirect/target");
}

TEST_F(IntegrationRedirectTest, RedirectSupportsPermanent301) {
    const auto res = client->Get("/api/v1/test/redirect/old-301");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 301);
    EXPECT_EQ(res->header("Location"), "/api/v1/test/redirect/target");
}

TEST_F(IntegrationRedirectTest, RedirectHonoursMultipleMethodConstraints) {
    const auto getRes = client->Get("/api/v1/test/redirect/old-multi");
    ASSERT_TRUE(getRes);
    EXPECT_EQ(getRes->status, 307);
    EXPECT_EQ(getRes->header("Location"), "/api/v1/test/redirect/target");

    const auto postRes = client->Post("/api/v1/test/redirect/old-multi", "{}", "application/json");
    ASSERT_TRUE(postRes);
    EXPECT_EQ(postRes->status, 307);
    EXPECT_EQ(postRes->header("Location"), "/api/v1/test/redirect/target");
}

TEST_F(IntegrationRedirectTest, RedirectDefaultConstraintIsGetOnly) {
    // The default redirect only allows GET; other methods must not redirect.
    const auto postRes = client->Post("/api/v1/test/redirect/old", "{}", "application/json");
    ASSERT_TRUE(postRes);
    EXPECT_NE(postRes->status, 302);
    EXPECT_TRUE(postRes->header("Location").empty());
}

TEST_F(IntegrationRedirectTest, RedirectRejectsInvalidMethodConstraint) {
    EXPECT_THROW(mantis().router().redirect("/api/v1/test/redirect/bad",
                                             "/api/v1/test/redirect/target",
                                             302,
                                             {"BREW"}),
                 mb::MantisException);
}

TEST_F(IntegrationRedirectTest, RedirectEmptyConstraintsAllowAllMethods) {
    const auto getRes = client->Get("/api/v1/test/redirect/allow-all");
    ASSERT_TRUE(getRes);
    EXPECT_EQ(getRes->status, 302);
    EXPECT_EQ(getRes->header("Location"), "/api/v1/test/redirect/target");

    const auto postRes = client->Post("/api/v1/test/redirect/allow-all", "{}", "application/json");
    ASSERT_TRUE(postRes);
    EXPECT_EQ(postRes->status, 302);
    EXPECT_EQ(postRes->header("Location"), "/api/v1/test/redirect/target");
}
