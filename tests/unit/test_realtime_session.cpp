#include <gtest/gtest.h>
#include <drogon/HttpRequest.h>
#include "mantisbase/core/realtime_session.h"
#include "mantisbase/core/models/access_rules.h"

TEST(RealtimeSession, GuestSnapshotIsGuest) {
    const auto snap = mb::makeGuestAuthSnapshot();
    EXPECT_FALSE(mb::isAuthenticatedSnapshot(snap));
    EXPECT_TRUE(mb::isGuestAuth(snap.auth));
}

TEST(RealtimeSession, TryUpgradeGuestToAuthed) {
    auto session = mb::makeGuestAuthSnapshot();
    mb::RealtimeAuthSnapshot incoming;
    incoming.auth = {
        {"type", "user"},
        {"entity", "users"},
        {"id", "user-1"},
        {"user", {{"id", "user-1"}, {"entity", "users"}}}
    };
    incoming.verification = {{"verified", true}, {"error", ""}};

    EXPECT_EQ(mb::tryUpgradeAuth(session, incoming), mb::AuthUpgradeResult::Applied);
    EXPECT_TRUE(mb::isAuthenticatedSnapshot(session));
}

TEST(RealtimeSession, TryUpgradeDeniesDowngrade) {
    mb::RealtimeAuthSnapshot session;
    session.auth = {
        {"type", "user"},
        {"entity", "users"},
        {"id", "user-1"},
        {"user", {{"id", "user-1"}, {"entity", "users"}}}
    };
    session.verification = {{"verified", true}, {"error", ""}};

    const auto guest = mb::makeGuestAuthSnapshot();
    EXPECT_EQ(mb::tryUpgradeAuth(session, guest), mb::AuthUpgradeResult::DeniedDowngrade);
}

TEST(RealtimeSession, TryUpgradeDeniesUserSwitch) {
    mb::RealtimeAuthSnapshot session;
    session.auth = {
        {"type", "user"},
        {"entity", "users"},
        {"id", "user-1"},
        {"user", {{"id", "user-1"}, {"entity", "users"}}}
    };
    session.verification = {{"verified", true}, {"error", ""}};

    mb::RealtimeAuthSnapshot incoming;
    incoming.auth = {
        {"type", "user"},
        {"entity", "users"},
        {"id", "user-2"},
        {"user", {{"id", "user-2"}, {"entity", "users"}}}
    };
    incoming.verification = {{"verified", true}, {"error", ""}};

    EXPECT_EQ(mb::tryUpgradeAuth(session, incoming), mb::AuthUpgradeResult::DeniedSwitch);
}

TEST(RealtimeSession, GenerateClientIdUsesPrefix) {
    const auto id = mb::generateRealtimeClientId("rt_sse");
    EXPECT_TRUE(id.starts_with("rt_sse_"));
}

TEST(RealtimeSession, ResolveRealtimeTokenQueryOverridesHeader) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime");
    req->setParameter("token", "query-token");
    req->addHeader("Authorization", "Bearer header-token");
    EXPECT_EQ(mb::resolveRealtimeToken(req), "query-token");
}

TEST(RealtimeSession, ResolveRealtimeTokenUsesBearerHeader) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/api/v1/realtime");
    req->addHeader("Authorization", "Bearer header-token");
    EXPECT_EQ(mb::resolveRealtimeToken(req), "header-token");
}
