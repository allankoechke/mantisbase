#include <gtest/gtest.h>
#include "mantisbase/core/models/access_rules.h"
#include "mantisbase/core/exceptions.h"
#include <nlohmann/json.hpp>

namespace {
    nlohmann::json verifiedUserAuth(const std::string &entity = "users") {
        return {
            {"type", "user"},
            {"entity", entity},
            {"id", "user-1"},
            {"user", {{"id", "user-1"}, {"entity", entity}}}
        };
    }

    nlohmann::json verifiedAdminAuth() {
        return {
            {"type", "admin"},
            {"entity", "mb_admins"},
            {"id", "admin-1"},
            {"user", {{"id", "admin-1"}, {"entity", "mb_admins"}}}
        };
    }

    nlohmann::json verifiedJson() {
        return {{"verified", true}, {"error", ""}};
    }
}

TEST(AuthTypeHelpers, GuestAdminUser) {
    EXPECT_TRUE(mb::isGuestAuth(nlohmann::json::object()));
    EXPECT_TRUE(mb::isGuestAuth({{"type", "guest"}}));
    EXPECT_FALSE(mb::isAdminAuth({{"type", "guest"}}));
    EXPECT_FALSE(mb::isUserAuth({{"type", "guest"}}));

    EXPECT_TRUE(mb::isAdminAuth({{"type", "admin"}}));
    EXPECT_TRUE(mb::isUserAuth({{"type", "user"}}));
}

TEST(MbAccessRule, DefaultConstructor) {
    const mb::MbAccessRule rule;

    EXPECT_EQ(rule.mode(), "");
    EXPECT_EQ(rule.expr(), "");
    EXPECT_EQ(rule.entity(), "");
}

TEST(MbAccessRule, AuthEntityFilter) {
    const mb::MbAccessRule rule("auth", "", "users,editors");
    EXPECT_EQ(rule.mode(), "auth");
    EXPECT_EQ(rule.entity(), "users,editors");
    EXPECT_TRUE(rule.matchesAuthEntity("users"));
    EXPECT_TRUE(rule.matchesAuthEntity("editors"));
    EXPECT_FALSE(rule.matchesAuthEntity("guests"));

    const mb::MbAccessRule negRule("auth", "", "!guests");
    EXPECT_TRUE(negRule.matchesAuthEntity("users"));
    EXPECT_FALSE(negRule.matchesAuthEntity("guests"));
}

TEST(MbAccessRule, AuthEntityValidation) {
    EXPECT_THROW(mb::MbAccessRule("auth", "", "bad name"), mb::MbException);
    EXPECT_THROW(mb::MbAccessRule("public", "", "users"), mb::MbException);

    mb::MbAccessRule rule("public");
    EXPECT_THROW(rule.setEntity("users"), mb::MbException);
    rule.setMode("auth");
    EXPECT_NO_THROW(rule.setEntity("users"));
}

TEST(MbAccessRule, EntityJsonRoundTrip) {
    nlohmann::json j = {
        {"mode", "auth"},
        {"entity", "users,editors"}
    };

    const auto rule = mb::MbAccessRule::fromJSON(j);
    EXPECT_EQ(rule.entity(), "users,editors");

    const auto out = rule.toJSON();
    EXPECT_EQ(out["mode"], "auth");
    EXPECT_EQ(out["entity"], "users,editors");
    EXPECT_TRUE(out.contains("expr"));

    const mb::MbAccessRule wildcard("auth");
    EXPECT_FALSE(wildcard.toJSON().contains("entity"));
}

TEST(MbAccessRule, FromJSONRejectsNonStringEntity) {
    nlohmann::json j = {
        {"mode", "auth"},
        {"entity", nlohmann::json::array({"users"})}
    };
    EXPECT_THROW(mb::MbAccessRule::fromJSON(j), mb::MbException);
}

TEST(MbAccessRule, ConstructorWithParams) {
    const mb::MbAccessRule rule("custom", "@auth.id != \"\"");

    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "@auth.id != \"\"");
}

TEST(MbAccessRule, SetModeAndExpr) {
    mb::MbAccessRule rule;

    rule.setMode("public");
    rule.setExpr("");

    EXPECT_EQ(rule.mode(), "public");
    EXPECT_EQ(rule.expr(), "");

    rule.setMode("auth");
    rule.setEntity("users");
    EXPECT_EQ(rule.entity(), "users");

    rule.setMode("custom");
    EXPECT_EQ(rule.entity(), "");
    rule.setExpr("@auth.entity == \"mb_admins\"");
    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "@auth.entity == \"mb_admins\"");
}

TEST(MbAccessRule, ToJSON) {
    const mb::MbAccessRule rule("custom", "@auth.id == '123'");
    auto json = rule.toJSON();

    EXPECT_EQ(json["mode"], "custom");
    EXPECT_EQ(json["expr"], "@auth.id == '123'");
}

TEST(MbAccessRule, FromJSON) {
    nlohmann::json j = {
        {"mode", "public"},
        {"expr", ""}
    };

    auto rule = mb::MbAccessRule::fromJSON(j);

    EXPECT_EQ(rule.mode(), "public");
    EXPECT_EQ(rule.expr(), "");

    j = {
        {"mode", "custom"},
        {"expr", "@auth.id != null"}
    };

    rule = mb::MbAccessRule::fromJSON(j);
    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "@auth.id != null");
}

TEST(MbAccessRule, RoundTripJSON) {
    mb::MbAccessRule original("custom", "@auth.entity == \"users\"");

    const auto json = original.toJSON();
    const auto restored = mb::MbAccessRule::fromJSON(json);

    EXPECT_EQ(restored.mode(), original.mode());
    EXPECT_EQ(restored.expr(), original.expr());
}

TEST(MbAccessRule, DifferentModes) {
    mb::MbAccessRule publicRule("public", "");
    EXPECT_EQ(publicRule.mode(), "public");

    mb::MbAccessRule authRule("auth", "");
    EXPECT_EQ(authRule.mode(), "auth");

    mb::MbAccessRule customRule("custom", "@auth.id == @req.body.user_id");
    EXPECT_EQ(customRule.mode(), "custom");
    EXPECT_EQ(customRule.expr(), "@auth.id == @req.body.user_id");

    mb::MbAccessRule adminRule("", "");
    EXPECT_EQ(adminRule.mode(), "");
    EXPECT_EQ(adminRule.expr(), "");
}

TEST(EvaluateAccessRule, AdminShortCircuits) {
    const mb::MbAccessRule customRule("custom", "false");
    const mb::MbAccessRule authRule("auth", "", "users");

    mb::MbAccessEvalContext ctx{verifiedAdminAuth(), verifiedJson(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(customRule, ctx), mb::MbAccessEvalResult::Allow);
    EXPECT_EQ(mb::evaluateAccessRule(authRule, ctx), mb::MbAccessEvalResult::Allow);
}

TEST(EvaluateAccessRule, PublicAllowsGuest) {
    const mb::MbAccessRule rule("public");
    mb::MbAccessEvalContext ctx{nlohmann::json{{"type", "guest"}}, nlohmann::json::object(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(rule, ctx), mb::MbAccessEvalResult::Allow);
}

TEST(EvaluateAccessRule, AuthEntityFilter) {
    const mb::MbAccessRule rule("auth", "", "users");
    mb::MbAccessEvalContext allowed{verifiedUserAuth("users"), verifiedJson(), nullptr};
    mb::MbAccessEvalContext denied{verifiedUserAuth("editors"), verifiedJson(), nullptr};

    EXPECT_EQ(mb::evaluateAccessRule(rule, allowed), mb::MbAccessEvalResult::Allow);
    EXPECT_EQ(mb::evaluateAccessRule(rule, denied), mb::MbAccessEvalResult::DenyForbidden);
}

TEST(EvaluateAccessRule, AdminOnlyDeniesUser) {
    const mb::MbAccessRule rule("");
    mb::MbAccessEvalContext ctx{verifiedUserAuth("users"), verifiedJson(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(rule, ctx), mb::MbAccessEvalResult::DenyForbidden);
}

TEST(EvaluateAccessRule, AuthRequiresVerification) {
    const mb::MbAccessRule rule("auth");
    mb::MbAccessEvalContext ctx{verifiedUserAuth("users"), nlohmann::json::object(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(rule, ctx), mb::MbAccessEvalResult::DenyUnauthenticated);
}
